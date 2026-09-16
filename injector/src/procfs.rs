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

pub fn is_injector_alive(pid: Pid) -> bool {
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

pub fn find_injector_pid() -> Option<Pid> {
    for entry in fs::read_dir("/proc").ok()?.flatten() {
        let name = entry.file_name();
        let Ok(pid) = name.to_string_lossy().parse::<Pid>() else {
            continue;
        };
        if is_injector_alive(pid) {
            return Some(pid);
        }
    }
    None
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
