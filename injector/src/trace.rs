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

use std::fs;
use std::path::Path;

use crate::arch::{Arch, Regs};
use crate::daemon::Daemon;
use crate::elf::{self, ElfHeader};
use crate::log::{loge, logi, logw};
use crate::maps;
use crate::procfs;
use crate::ptrace_ops::{
    read_c_string, read_memory, read_registers, setup_call, write_memory, write_registers,
};
use crate::sys::Pid;
use crate::sys::ptrace;
use crate::sys::signal;
use crate::sys::wait::Status;
use crate::targets;

const INSTRUCTION_CAPACITY: usize = 8;
const STACK_MASK: usize = !0xF;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum State {
    Traced,
    Entry,
    Memfd,
    Dlopen,
    Dlsym,
    Init,
    Dlerror,
}

#[derive(Debug, Clone)]
pub struct Tracee {
    pub state: State,
    pub arch: Arch,
    pub entry: usize,
    original_instruction: [u8; INSTRUCTION_CAPACITY],
    breakpoint_size: usize,
    saved: Regs,
    pub loader: String,
    content: Vec<u8>,
    pub exe: String,
    pub deadline_ms: u64,
}

impl Tracee {
    pub fn new(state: State, arch: Arch) -> Self {
        Self {
            state,
            arch,
            entry: 0,
            original_instruction: [0; INSTRUCTION_CAPACITY],
            breakpoint_size: 0,
            saved: Regs::new(arch),
            loader: String::new(),
            content: Vec::new(),
            exe: String::new(),
            deadline_ms: 0,
        }
    }
}

enum Progress {
    Tracing,
    Finished,
}

impl Daemon {
    pub fn handle_event(&mut self, pid: Pid, status: Status) {
        if status.is_exited() || status.is_signaled() {
            self.tracees.remove(&pid);
            return;
        }
        if !status.is_stopped() {
            return;
        }

        let crash_signal = status.stop_signal();
        if crash_signal == signal::SIGTRAP {
            let event = status.trap_event();
            if event == ptrace::EVENT_STOP {
                ptrace::cont(pid, None);
                return;
            }
            match event {
                ptrace::EVENT_FORK | ptrace::EVENT_VFORK | ptrace::EVENT_CLONE => {
                    if let Ok(child) = ptrace::event_message(pid)
                        && child != 0
                    {
                        self.attach_child(child as Pid);
                    }
                    ptrace::cont(pid, None);
                }
                ptrace::EVENT_EXEC => self.handle_exec(pid),
                ptrace::EVENT_EXIT => {
                    self.tracees.remove(&pid);
                    ptrace::cont(pid, None);
                }
                _ => {
                    if self.tracees.contains_key(&pid) {
                        self.handle_trap(pid);
                    } else {
                        ptrace::cont(pid, None);
                    }
                }
            }
            return;
        }

        if crash_signal == signal::SIGSTOP || crash_signal == signal::SIGCHLD {
            ptrace::cont(pid, None);
            return;
        }

        self.report_fault(pid, crash_signal);
        ptrace::cont(pid, Some(crash_signal));
    }

    pub fn attach_child(&mut self, child: Pid) {
        self.tracees
            .entry(child)
            .or_insert_with(|| Tracee::new(State::Traced, Arch::Unknown));
    }

    fn handle_exec(&mut self, pid: Pid) {
        let Some(mut tracee) = self.tracees.remove(&pid) else {
            ptrace::detach(pid, None);
            return;
        };
        if self.arm_entry_breakpoint(pid, &mut tracee) {
            self.tracees.insert(pid, tracee);
            ptrace::cont(pid, None);
        } else {
            ptrace::detach(pid, None);
        }
    }

    fn arm_entry_breakpoint(&mut self, pid: Pid, tracee: &mut Tracee) -> bool {
        tracee.exe = procfs::read_exe_path(pid);
        if tracee.exe.is_empty() {
            return false;
        }
        if !targets::matches(&self.targets, &tracee.exe) {
            return false;
        }

        if let Ok(context) = fs::read_to_string(format!("/proc/{pid}/attr/current")) {
            let context = context.trim_end_matches(['\0', '\n', '\r']);
            logi!("SELinux context of {} (pid {pid}): {context}", tracee.exe);
        }

        let Some(header) = elf::header_of_file(Path::new(&format!("/proc/{pid}/exe"))) else {
            return false;
        };
        tracee.arch = elf::arch_of(&header);
        if tracee.arch == Arch::Unknown {
            logw!(
                "skipping {} (pid {pid}): unsupported machine {}",
                tracee.exe,
                header.machine
            );
            return false;
        }

        let base = maps::parse_for_pid(pid)
            .into_iter()
            .find(|entry| {
                entry.offset == 0 && procfs::map_path_equals_exe(&entry.path, &tracee.exe)
            })
            .map_or(0, |entry| entry.start);
        let (entry, thumb) = entry_point(tracee.arch, &header, base);
        if entry == 0 {
            return false;
        }

        tracee.entry = entry;
        tracee.loader = loader_path(self, header.is_64);
        tracee.state = State::Entry;
        if !set_entry_breakpoint(pid, tracee, thumb) {
            return false;
        }

        logi!(
            "injecting {} into {} (pid {pid}) at entry {entry:#x} ({})",
            tracee.loader,
            tracee.exe,
            if header.is_64 { "64-bit" } else { "32-bit" }
        );
        true
    }

    fn handle_trap(&mut self, pid: Pid) {
        let Some(mut tracee) = self.tracees.remove(&pid) else {
            return;
        };
        if let Progress::Tracing = self.step(pid, &mut tracee) {
            self.tracees.insert(pid, tracee);
        }
    }

    fn step(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        match tracee.state {
            State::Entry => self.trap_at_entry(pid, tracee),
            State::Memfd => self.trap_after_memfd(pid, tracee),
            State::Dlopen => self.trap_after_dlopen(pid, tracee),
            State::Dlsym => self.trap_after_dlsym(pid, tracee),
            State::Dlerror => self.trap_after_dlerror(pid, tracee),
            State::Init => {
                logi!("loader initialized in pid {pid}");
                self.record_success(pid, &tracee.exe);
                restore_entry(pid, tracee);
                write_registers(pid, &tracee.saved);
                ptrace::detach(pid, None);
                Progress::Finished
            }
            State::Traced => {
                ptrace::cont(pid, None);
                Progress::Tracing
            }
        }
    }

    fn trap_at_entry(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let regs = match read_registers(pid, tracee.arch) {
            Ok(regs) => regs,
            Err(error) => {
                loge!("failed to get regs at entry for pid {pid}: {error}");
                restore_entry(pid, tracee);
                ptrace::detach(pid, None);
                return Progress::Finished;
            }
        };

        let mut pc = regs.pc();
        let mut expected = tracee.entry;
        if tracee.arch == Arch::X86 || tracee.arch == Arch::X86_64 {
            expected = expected.wrapping_add(1);
        } else if tracee.arch == Arch::Arm32 {
            pc &= !1;
        }
        if pc != expected {
            loge!("unexpected entry state for pid {pid} (pc={pc:#x}, expected={expected:#x})");
            restore_entry(pid, tracee);
            ptrace::detach(pid, None);
            return Progress::Finished;
        }

        tracee.saved = regs;
        if !self.start_memfd_call(pid, tracee) {
            restore_entry(pid, tracee);
            ptrace::detach(pid, None);
            return Progress::Finished;
        }
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn start_memfd_call(&self, pid: Pid, tracee: &mut Tracee) -> bool {
        let Some(syscall_address) = resolve_syscall(pid) else {
            loge!("failed to resolve syscall for pid {pid}");
            return false;
        };

        let mut regs = tracee.saved;
        let name = b"loader\0";
        let stack = regs.sp().wrapping_sub(name.len() + 0x10) & STACK_MASK;
        if !write_memory(pid, stack, name) {
            return false;
        }
        regs.set_sp(stack);

        let arguments = [tracee.arch.memfd_create_number() as usize, stack, 0];
        if !setup_call(pid, &mut regs, syscall_address, tracee.entry, &arguments) {
            return false;
        }
        if !write_registers(pid, &regs) {
            return false;
        }
        tracee.state = State::Memfd;
        true
    }

    fn trap_after_memfd(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let regs = match read_registers(pid, tracee.arch) {
            Ok(regs) => regs,
            Err(_) => {
                self.abort(pid, tracee, "failed to get regs after memfd_create");
                return Progress::Finished;
            }
        };

        let memfd = regs.return_value() as i32;
        if memfd < 0 {
            loge!("memfd_create failed for pid {pid} ({memfd})");
            self.give_up(pid, tracee, "memfd_create failed");
            return Progress::Finished;
        }

        if tracee.content.is_empty() {
            match procfs::read_file(Path::new(&tracee.loader)) {
                Some(content) => tracee.content = content,
                None => {
                    self.abort(pid, tracee, "cannot read the loader (injector side)");
                    return Progress::Finished;
                }
            }
        }
        if !procfs::write_to_target_fd(pid, memfd, &tracee.content) {
            loge!("failed to write to memfd {memfd} of pid {pid}");
            self.give_up(pid, tracee, "cannot write loader to memfd");
            return Progress::Finished;
        }

        let Some(dlopen_address) = resolve_dlopen_ext(pid) else {
            self.abort(pid, tracee, "cannot resolve android_dlopen_ext");
            return Progress::Finished;
        };

        let mut extinfo = [0u8; 48];
        extinfo[..8].copy_from_slice(&ANDROID_DLEXT_USE_LIBRARY_FD.to_ne_bytes());
        let library_fd_offset = if tracee.arch.is_64() { 28 } else { 20 };
        extinfo[library_fd_offset..library_fd_offset + 4].copy_from_slice(&memfd.to_ne_bytes());

        let name = b"libloader.so\0";
        let name_size = (name.len() + 7) & !7;
        let mut regs = regs;
        let stack = regs.sp().wrapping_sub(name_size + extinfo.len() + 0x10) & STACK_MASK;
        let name_address = stack;
        let extinfo_address = stack + name_size;
        if !write_memory(pid, name_address, name) || !write_memory(pid, extinfo_address, &extinfo) {
            self.abort(pid, tracee, "cannot write dlopen args");
            return Progress::Finished;
        }
        regs.set_sp(stack);

        let arguments = [name_address, RTLD_NOW, extinfo_address, tracee.entry];
        if !setup_call(pid, &mut regs, dlopen_address, tracee.entry, &arguments)
            || !write_registers(pid, &regs)
        {
            self.abort(pid, tracee, "cannot setup android_dlopen_ext");
            return Progress::Finished;
        }

        tracee.state = State::Dlopen;
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn trap_after_dlopen(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let regs = match read_registers(pid, tracee.arch) {
            Ok(regs) => regs,
            Err(_) => {
                restore_entry(pid, tracee);
                ptrace::detach(pid, None);
                return Progress::Finished;
            }
        };

        let handle = regs.return_value();
        if handle == 0 {
            let Some(dlerror_address) = resolve_dlerror(pid) else {
                loge!(
                    "dlopen({}) failed for pid {pid} (cannot resolve/call dlerror)",
                    tracee.loader
                );
                self.give_up(pid, tracee, "dlopen failed (cannot resolve dlerror)");
                return Progress::Finished;
            };
            let mut regs = regs;
            if !setup_call(pid, &mut regs, dlerror_address, tracee.entry, &[]) {
                loge!(
                    "dlopen({}) failed for pid {pid} (cannot resolve/call dlerror)",
                    tracee.loader
                );
                self.give_up(pid, tracee, "dlopen failed (cannot resolve dlerror)");
                return Progress::Finished;
            }
            write_registers(pid, &regs);
            tracee.state = State::Dlerror;
            ptrace::cont(pid, None);
            return Progress::Tracing;
        }

        logi!("loader injected into pid {pid} (handle {handle:#x})");

        let Some(dlsym_address) = resolve_symbol(pid, "libdl.so", "dlsym") else {
            loge!("failed to resolve dlsym for pid {pid}");
            self.give_up(pid, tracee, "cannot resolve dlsym");
            return Progress::Finished;
        };

        let mut regs = regs;
        let name = b"znn_loader_init\0";
        let stack = regs.sp().wrapping_sub(name.len() + 0x10) & STACK_MASK;
        if !write_memory(pid, stack, name) {
            loge!("failed to write init symbol name for pid {pid}");
            self.undo(pid, tracee);
            return Progress::Finished;
        }
        regs.set_sp(stack);

        let arguments = [handle, stack, 0, 0];
        if !setup_call(pid, &mut regs, dlsym_address, tracee.entry, &arguments[..2])
            || !write_registers(pid, &regs)
        {
            loge!("failed to setup dlsym for pid {pid}");
            self.undo(pid, tracee);
            return Progress::Finished;
        }

        tracee.state = State::Dlsym;
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn trap_after_dlsym(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let regs = match read_registers(pid, tracee.arch) {
            Ok(regs) => regs,
            Err(_) => {
                loge!("failed to get regs after dlsym for pid {pid}");
                restore_entry(pid, tracee);
                ptrace::detach(pid, None);
                return Progress::Finished;
            }
        };

        let init_function = regs.return_value();
        if init_function == 0 {
            loge!("dlsym(znn_loader_init) failed for pid {pid}");
            self.give_up(pid, tracee, "dlsym znn_loader_init failed");
            return Progress::Finished;
        }

        let mut regs = regs;
        if !setup_call(pid, &mut regs, init_function, tracee.entry, &[])
            || !write_registers(pid, &regs)
        {
            loge!("failed to setup znn_loader_init for pid {pid}");
            self.undo(pid, tracee);
            return Progress::Finished;
        }

        tracee.state = State::Init;
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn trap_after_dlerror(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let regs = match read_registers(pid, tracee.arch) {
            Ok(regs) => regs,
            Err(_) => {
                restore_entry(pid, tracee);
                ptrace::detach(pid, None);
                return Progress::Finished;
            }
        };

        let error_pointer = regs.return_value();
        let error = if error_pointer == 0 {
            "(null)".to_owned()
        } else {
            read_c_string(pid, error_pointer)
        };
        loge!("dlopen({}) failed for pid {pid}: {error}", tracee.loader);
        self.record_failure(pid, &tracee.exe, &format!("dlopen failed: {error}"));

        if error.contains("Permission denied") {
            loge!(
                "hint for pid {pid}: dlopen needs the `execute` permission on the memfd's SELinux label (\"tmpfs\", \"unlabeled\", or the target's own <domain>_tmpfs, e.g. artd_tmpfs). Make sure the module sepolicy.rule is applied; Zygisk Next Next ships `allow * * file execute` to cover every label."
            );
        } else if error.contains("not accessible") {
            loge!(
                "hint for pid {pid}: the linker rejected the library path for its namespace (loading must go through a tmpfs memfd)"
            );
        }

        self.undo(pid, tracee);
        Progress::Finished
    }

    fn abort(&mut self, pid: Pid, tracee: &Tracee, reason: &str) {
        loge!("{reason} (pid {pid})");
        self.give_up(pid, tracee, reason);
    }

    fn give_up(&mut self, pid: Pid, tracee: &Tracee, reason: &str) {
        self.record_failure(pid, &tracee.exe, reason);
        self.undo(pid, tracee);
    }

    fn undo(&self, pid: Pid, tracee: &Tracee) {
        restore_entry(pid, tracee);
        write_registers(pid, &tracee.saved);
        ptrace::detach(pid, None);
    }

    fn report_fault(&self, pid: Pid, crash_signal: i32) {
        const FATAL: [i32; 6] = [
            signal::SIGSEGV,
            signal::SIGBUS,
            signal::SIGABRT,
            signal::SIGILL,
            signal::SIGFPE,
            signal::SIGSYS,
        ];
        if !FATAL.contains(&crash_signal) {
            return;
        }
        let Some(tracee) = self.tracees.get(&pid) else {
            return;
        };
        let Ok(regs) = read_registers(pid, tracee.arch) else {
            return;
        };

        let pc = regs.pc();
        let sp = regs.sp();
        let link = regs.link().unwrap_or(0);
        let location = maps::parse_for_pid(pid)
            .into_iter()
            .find(|entry| entry.contains(pc))
            .map_or_else(
                || "(unmapped)".to_owned(),
                |entry| format!("{}+0x{:x}", entry.path, pc - entry.start),
            );
        loge!(
            "fatal signal {crash_signal} in pid {pid} at pc={pc:#x} lr={link:#x} sp={sp:#x} in {location}"
        );
    }
}

pub(crate) const ANDROID_DLEXT_USE_LIBRARY_FD: u64 = 0x10;
pub(crate) const RTLD_NOW: usize = 2;

fn loader_path(daemon: &Daemon, is_64: bool) -> String {
    let path = if is_64 {
        &daemon.loader64
    } else {
        &daemon.loader32
    };
    path.to_string_lossy().into_owned()
}

pub(crate) fn entry_point(arch: Arch, header: &ElfHeader, base: usize) -> (usize, bool) {
    let mut entry = header.entry as usize;
    let mut thumb = false;
    if arch == Arch::Arm32 && entry & 1 != 0 {
        thumb = true;
        entry &= !1;
    }
    if header.is_position_independent() {
        entry = entry.wrapping_add(base);
    }
    (entry, thumb)
}

fn find_library(pid: Pid, basename: &str) -> Option<(usize, String)> {
    maps::parse_for_pid(pid)
        .into_iter()
        .find(|entry| entry.offset == 0 && procfs::basename(&entry.path) == basename)
        .map(|entry| (entry.start, entry.path))
}

pub(crate) fn resolve_symbol(pid: Pid, basename: &str, symbol: &str) -> Option<usize> {
    let (base, path) = find_library(pid, basename)?;
    let address = elf::symbol_address(Path::new(&path), base, symbol);
    (address != 0).then_some(address)
}

pub(crate) fn resolve_dlopen_ext(pid: Pid) -> Option<usize> {
    let symbols = [
        ("linker64", "__loader_android_dlopen_ext"),
        ("linker", "__loader_android_dlopen_ext"),
        ("libdl.so", "android_dlopen_ext"),
    ];
    symbols
        .iter()
        .find_map(|(library, symbol)| resolve_symbol(pid, library, symbol))
}

pub(crate) fn resolve_dlerror(pid: Pid) -> Option<usize> {
    resolve_symbol(pid, "libdl.so", "dlerror")
}

pub(crate) fn resolve_syscall(pid: Pid) -> Option<usize> {
    resolve_symbol(pid, "libc.so", "syscall")
}

pub(crate) fn set_entry_breakpoint(pid: Pid, tracee: &mut Tracee, thumb: bool) -> bool {
    let instruction = tracee.arch.breakpoint(thumb);
    if instruction.is_empty() || instruction.len() > INSTRUCTION_CAPACITY {
        return false;
    }
    let size = instruction.len();
    if !read_memory(pid, tracee.entry, &mut tracee.original_instruction[..size]) {
        return false;
    }
    if !write_memory(pid, tracee.entry, instruction) {
        return false;
    }
    tracee.breakpoint_size = size;
    true
}

pub(crate) fn restore_entry(pid: Pid, tracee: &Tracee) {
    if tracee.breakpoint_size != 0 {
        write_memory(
            pid,
            tracee.entry,
            &tracee.original_instruction[..tracee.breakpoint_size],
        );
    }
}
