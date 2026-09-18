/*
 * This file is part of Zygisk Next Next.
 *
 * Zygisk Next Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zygisk Next Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zygisk Next Next. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 VeryBaaad <verybaaad@outlook.com>
 */

use std::fs::{self, File};
use std::io::{Read, Write};
use std::os::fd::RawFd;
use std::path::Path;
use std::sync::OnceLock;
use std::time::Instant;

use crate::maps::{self, PROT_EXEC};
use crate::sys::Pid;

const DELETED_SUFFIX: &str = " (deleted)";

pub fn now_ms() -> u64 {
    static START: OnceLock<Instant> = OnceLock::new();
    START.get_or_init(Instant::now).elapsed().as_millis() as u64
}

pub fn basename(path: &str) -> &str {
    match path.rfind('/') {
        Some(index) => &path[index + 1..],
        None => path,
    }
}

pub fn process_name(pid: Pid, exe: &str) -> String {
    let mut buffer = [0u8; 511];
    if let Ok(mut file) = File::open(format!("/proc/{pid}/cmdline"))
        && let Ok(read) = file.read(&mut buffer)
    {
        let end = buffer[..read]
            .iter()
            .position(|byte| *byte == 0)
            .unwrap_or(read);
        let name = String::from_utf8_lossy(&buffer[..end]);
        let name = name.trim_matches(|c: char| c.is_ascii_whitespace());
        if !name.is_empty() {
            return name.to_owned();
        }
    }
    basename(exe).to_owned()
}

pub fn strip_deleted_suffix(mut path: String) -> String {
    if path.len() > DELETED_SUFFIX.len() && path.ends_with(DELETED_SUFFIX) {
        path.truncate(path.len() - DELETED_SUFFIX.len());
    }
    path
}

pub fn read_exe_path(pid: Pid) -> String {
    match fs::read_link(format!("/proc/{pid}/exe")) {
        Ok(path) => strip_deleted_suffix(path.to_string_lossy().into_owned()),
        Err(_) => String::new(),
    }
}

pub fn map_path_equals_exe(map_path: &str, exe: &str) -> bool {
    map_path == exe
        || (map_path.len() == exe.len() + DELETED_SUFFIX.len()
            && map_path.starts_with(exe)
            && map_path[exe.len()..] == *DELETED_SUFFIX)
}

fn is_injector_process(pid: Pid) -> bool {
    if pid <= 1 {
        return false;
    }
    let Ok(mut file) = File::open(format!("/proc/{pid}/comm")) else {
        return false;
    };
    let mut comm = [0u8; 63];
    let Ok(read) = file.read(&mut comm) else {
        return false;
    };
    comm[..read].starts_with(b"injector")
}

fn command_line(pid: Pid) -> Vec<String> {
    let Ok(contents) = fs::read(format!("/proc/{pid}/cmdline")) else {
        return Vec::new();
    };
    contents
        .split(|byte| *byte == 0)
        .filter(|argument| !argument.is_empty())
        .map(|argument| String::from_utf8_lossy(argument).into_owned())
        .collect()
}

/// The control client is the same binary as the daemon, so the process name
/// cannot tell them apart: the daemon is the instance that was started without
/// a control command. Never report ourselves.
pub fn is_daemon_alive(pid: Pid) -> bool {
    if pid <= 1 || pid == std::process::id() as Pid || !is_injector_process(pid) {
        return false;
    }
    !command_line(pid).iter().any(|argument| argument == "--ctl")
}

/// Every thread of a process, lowest tid first.
pub fn thread_ids(pid: Pid) -> Vec<Pid> {
    let Ok(entries) = fs::read_dir(format!("/proc/{pid}/task")) else {
        return Vec::new();
    };
    let mut tids: Vec<Pid> = entries
        .flatten()
        .filter_map(|entry| entry.file_name().to_string_lossy().parse::<Pid>().ok())
        .collect();
    tids.sort_unstable();
    tids
}

/// Whether a thread currently sits inside a syscall: `/proc/<tid>/syscall`
/// starts with the syscall number and prints `-1` for user space. `None` when
/// the kernel or the policy does not let us look.
pub fn in_syscall(tid: Pid) -> Option<bool> {
    let contents = fs::read_to_string(format!("/proc/{tid}/syscall")).ok()?;
    let number = contents.split_whitespace().next()?;
    number.parse::<i64>().ok().map(|number| number >= 0)
}

/// The program counter of a process, read without tracing it: `/proc/<pid>/syscall`
/// ends with the user stack pointer and the program counter, in hex. Returns
/// `None` when the kernel or the policy does not let us look, and callers must
/// then not assume anything about where the process is.
pub fn current_pc(pid: Pid) -> Option<usize> {
    let contents = fs::read_to_string(format!("/proc/{pid}/syscall")).ok()?;
    let pc = contents.split_whitespace().last()?;
    usize::from_str_radix(pc.trim_start_matches("0x"), 16).ok()
}

/// The pid that currently traces `pid`, when that tracer is not us. This is the
/// exact, name free way to notice another loader: a process can only ever have
/// one tracer, and a loader that took it is by definition a foreign tracer.
pub fn foreign_tracer(pid: Pid) -> Option<Pid> {
    let status = fs::read_to_string(format!("/proc/{pid}/status")).ok()?;
    let tracer = status
        .lines()
        .find_map(|line| line.strip_prefix("TracerPid:"))?
        .trim()
        .parse::<Pid>()
        .ok()?;
    if tracer <= 1 || tracer == std::process::id() as Pid {
        return None;
    }
    Some(tracer)
}

/// The executable of a process, for reporting which foreign loader holds it.
pub fn process_exe(pid: Pid) -> String {
    read_exe_path(pid)
}

/// Locate the running daemon, preferring the pid the daemon published in its
/// state snapshot. Several daemons can outlive a userspace restart, so the
/// oldest one (the lowest pid) wins when no pid was recorded.
pub fn find_daemon_pid(recorded: Option<Pid>) -> Option<Pid> {
    if recorded.is_some_and(is_daemon_alive) {
        return recorded;
    }
    let mut found: Option<Pid> = None;
    for entry in fs::read_dir("/proc").ok()?.flatten() {
        let name = entry.file_name();
        let Ok(pid) = name.to_string_lossy().parse::<Pid>() else {
            continue;
        };
        if !is_daemon_alive(pid) {
            continue;
        }
        found = Some(found.map_or(pid, |current| current.min(pid)));
    }
    found
}

pub fn is_pre_exec_fork(exe: &str) -> bool {
    basename(exe) == "init"
}

pub fn pc_in_exe_text(pid: Pid, exe: &str, pc: usize) -> bool {
    maps::parse_for_pid(pid).iter().any(|entry| {
        map_path_equals_exe(&entry.path, exe) && entry.perms & PROT_EXEC != 0 && entry.contains(pc)
    })
}

pub fn read_file(path: &Path) -> Option<Vec<u8>> {
    let bytes = fs::read(path).ok()?;
    if bytes.is_empty() { None } else { Some(bytes) }
}

pub fn write_to_target_fd(pid: Pid, fd: RawFd, data: &[u8]) -> bool {
    let Ok(mut file) = fs::OpenOptions::new()
        .write(true)
        .open(format!("/proc/{pid}/fd/{fd}"))
    else {
        return false;
    };
    file.write_all(data).is_ok()
}
