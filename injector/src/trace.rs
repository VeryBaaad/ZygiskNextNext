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
use crate::companion;
use crate::config;
use crate::daemon::Daemon;
use crate::elf::{self, ElfHeader};
use crate::log::{loge, logi, logw};
use crate::maps;
use crate::plan::{self, Plan, PlanModule};
use crate::procfs;
use crate::ptrace_ops::{
    read_c_string, read_memory, read_registers, setup_call, write_memory, write_registers,
};
use crate::sys;
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
    File,
    Dlopen,
    Dlsym,
    Init,
    Dlerror,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum FilePurpose {
    Loader,
    Module,
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
    init_function: usize,
    modules: Vec<PlanModule>,
    module_index: usize,
    nonce: u32,
    file_purpose: FilePurpose,
    file_fd: i32,
    file_name: Vec<u8>,
    module_content: Vec<u8>,
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
            init_function: 0,
            modules: Vec::new(),
            module_index: 0,
            nonce: 0,
            file_purpose: FilePurpose::Loader,
            file_fd: -1,
            file_name: Vec::new(),
            module_content: Vec::new(),
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
        tracee.deadline_ms = 0;
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
            State::File => self.trap_file(pid, tracee),
            State::Dlopen => self.trap_after_dlopen(pid, tracee),
            State::Dlsym => self.trap_after_dlsym(pid, tracee),
            State::Dlerror => self.trap_after_dlerror(pid, tracee),
            State::Init => {
                logi!("loader initialized in pid {pid}");
                self.record_success(pid, &tracee.exe);
                release_tracee(pid, tracee);
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
        if trap_reports_next_instruction(tracee.arch) {
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

        let mut regs = regs;
        if trap_reports_next_instruction(tracee.arch) {
            regs.set_pc(tracee.entry);
        }
        tracee.saved = regs;

        if !self.start_file(pid, tracee, FilePurpose::Loader, regs) {
            loge!("entry trap in pid {pid}: cannot start the loader image; giving up");
            release_tracee(pid, tracee);
            ptrace::detach(pid, None);
            return Progress::Finished;
        }
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn start_file(
        &mut self,
        pid: Pid,
        tracee: &mut Tracee,
        purpose: FilePurpose,
        regs: Regs,
    ) -> bool {
        if purpose == FilePurpose::Loader && tracee.content.is_empty() {
            match procfs::read_file(Path::new(&tracee.loader)) {
                Some(content) => tracee.content = content,
                None => {
                    loge!("cannot read the loader (injector side)");
                    return false;
                }
            }
        }

        tracee.file_purpose = purpose;
        tracee.file_fd = -1;
        let name = match purpose {
            FilePurpose::Loader => "znn-loader".to_owned(),
            FilePurpose::Module => format!("znn-module-{}", tracee.module_index),
        };
        tracee.file_name = name.into_bytes();
        tracee.file_name.push(0);

        self.start_memfd(pid, tracee, regs)
    }

    fn start_memfd(&self, pid: Pid, tracee: &mut Tracee, mut regs: Regs) -> bool {
        let Some(syscall_address) = resolve_syscall(pid) else {
            loge!("failed to resolve syscall for pid {pid}");
            return false;
        };
        let Some(name_address) = push_bytes(pid, &mut regs, &tracee.file_name) else {
            loge!("failed to write the memfd name below the stack for pid {pid}");
            return false;
        };
        let arguments = [
            tracee.arch.memfd_create_number() as usize,
            name_address,
            sys::memfd::MFD_CLOEXEC as usize,
        ];
        self.arm_call(pid, tracee, &mut regs, syscall_address, &arguments)
    }

    fn arm_call(
        &self,
        pid: Pid,
        tracee: &mut Tracee,
        regs: &mut Regs,
        function: usize,
        arguments: &[usize],
    ) -> bool {
        if !setup_call(pid, regs, function, tracee.entry, arguments) {
            return false;
        }
        tracee.state = State::File;
        write_registers(pid, regs)
    }

    fn trap_file(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let Ok(regs) = read_registers(pid, tracee.arch) else {
            self.abort(pid, tracee, "failed to get regs after memfd_create");
            return Progress::Finished;
        };

        let fd = regs.return_value() as i32;
        if fd < 0 {
            if fd == -libc::ENOSYS {
                loge!(
                    "pid {pid}: this kernel ({}) has no memfd_create, which needs Linux 3.17; \
                     Zygisk Next Next cannot inject here",
                    self.system.kernel
                );
                self.give_up(
                    pid,
                    tracee,
                    "kernel without memfd_create (needs Linux 3.17)",
                );
            } else {
                loge!("memfd_create failed for pid {pid} ({fd})");
                self.give_up(pid, tracee, "memfd_create failed");
            }
            return Progress::Finished;
        }

        tracee.file_fd = fd;
        self.file_ready(pid, tracee, regs)
    }

    fn file_ready(&mut self, pid: Pid, tracee: &mut Tracee, regs: Regs) -> Progress {
        let fd = tracee.file_fd;

        match tracee.file_purpose {
            FilePurpose::Loader => {
                if !procfs::write_to_target_fd(pid, fd, &tracee.content) {
                    self.abort(pid, tracee, "cannot fill the loader image");
                    return Progress::Finished;
                }
                self.start_dlopen(pid, tracee, regs, fd)
            }
            FilePurpose::Module => {
                if !procfs::write_to_target_fd(pid, fd, &tracee.module_content) {
                    self.abort(pid, tracee, "cannot fill a module library");
                    return Progress::Finished;
                }
                let module = tracee.module_index;
                logi!(
                    "pid {pid}: {} handed over as fd {fd} ({} bytes)",
                    tracee.modules[module].path,
                    tracee.module_content.len()
                );
                tracee.modules[module].fd = fd;
                tracee.module_index += 1;
                self.pump_modules(pid, tracee)
            }
        }
    }

    fn start_dlopen(&mut self, pid: Pid, tracee: &mut Tracee, mut regs: Regs, fd: i32) -> Progress {
        let Some(dlopen_address) = resolve_dlopen_ext(pid) else {
            self.abort(pid, tracee, "cannot resolve android_dlopen_ext");
            return Progress::Finished;
        };

        let mut extinfo = [0u8; 48];
        extinfo[..8].copy_from_slice(&ANDROID_DLEXT_USE_LIBRARY_FD.to_ne_bytes());
        let library_fd_offset = if tracee.arch.is_64() { 28 } else { 20 };
        extinfo[library_fd_offset..library_fd_offset + 4].copy_from_slice(&fd.to_ne_bytes());

        let name = b"libloader.so\0";
        let name_size = (name.len() + 7) & !7;
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
                release_tracee(pid, tracee);
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
                release_tracee(pid, tracee);
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
        tracee.init_function = init_function;

        tracee.modules = targets::libraries_for(&self.modules, &tracee.exe)
            .into_iter()
            .enumerate()
            .map(|(index, library)| PlanModule::new(index as u32, library.path, library.companion))
            .collect();
        tracee.module_index = 0;
        tracee.nonce = plan::fresh_nonce();
        logi!(
            "pid {pid}: handing {} module librar(y/ies) over to the loader",
            tracee.modules.len()
        );

        self.pump_modules(pid, tracee)
    }

    fn pump_modules(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        if tracee.module_index >= tracee.modules.len() {
            return self.finish_injection(pid, tracee);
        }

        let path = tracee.modules[tracee.module_index].path.clone();
        let Some(content) = procfs::read_file(Path::new(&path)) else {
            self.abort(pid, tracee, "cannot read a module library");
            return Progress::Finished;
        };
        tracee.module_content = content;

        let Ok(regs) = read_registers(pid, tracee.arch) else {
            self.abort(pid, tracee, "cannot read registers to start a module file");
            return Progress::Finished;
        };
        if !self.start_file(pid, tracee, FilePurpose::Module, regs) {
            self.abort(pid, tracee, "cannot start a module file in the target");
            return Progress::Finished;
        }
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn finish_injection(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let config = config::effective();
        let plan = Plan {
            modules: tracee.modules.clone(),
            inline_engine: config.inline_hook,
            plt_engine: config.plt_hook,
            nonce: tracee.nonce,
        };
        let Some(bytes) = plan.encode() else {
            self.abort(pid, tracee, "the boot plan does not fit");
            return Progress::Finished;
        };

        let Ok(mut regs) = read_registers(pid, tracee.arch) else {
            self.abort(pid, tracee, "cannot read registers to write the boot plan");
            return Progress::Finished;
        };
        let stack = regs.sp().wrapping_sub(bytes.len() + 0x10) & STACK_MASK;
        if !write_memory(pid, stack, &bytes) {
            self.abort(pid, tracee, "cannot write the boot plan into the target");
            return Progress::Finished;
        }
        regs.set_sp(stack);

        for module in plan.companions() {
            companion::spawn(&module.path, plan.nonce, module.index);
        }

        if !setup_call(pid, &mut regs, tracee.init_function, tracee.entry, &[stack])
            || !write_registers(pid, &regs)
        {
            self.abort(pid, tracee, "cannot enter znn_loader_init");
            return Progress::Finished;
        }
        logi!(
            "pid {pid}: boot plan ({} bytes, {} module(s), nonce {:#010x}) handed to the loader",
            bytes.len(),
            tracee.modules.len(),
            plan.nonce
        );

        tracee.state = State::Init;
        ptrace::cont(pid, None);
        Progress::Tracing
    }

    fn trap_after_dlerror(&mut self, pid: Pid, tracee: &mut Tracee) -> Progress {
        let regs = match read_registers(pid, tracee.arch) {
            Ok(regs) => regs,
            Err(_) => {
                release_tracee(pid, tracee);
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
                "hint for pid {pid}: the loader image is a memfd created inside the target, so the target's domain needs `execute` on its own tmpfs label. Zygisk Next Next ships the two rules that cover every label; make sure module/src/sepolicy.rule is applied."
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
        release_tracee(pid, tracee);
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

const ANDROID_DLEXT_USE_LIBRARY_FD: u64 = 0x10;
const RTLD_NOW: usize = 2;

fn push_bytes(pid: Pid, regs: &mut Regs, bytes: &[u8]) -> Option<usize> {
    let address = regs.sp().wrapping_sub(bytes.len() + 0x10) & STACK_MASK;
    if !write_memory(pid, address, bytes) {
        return None;
    }
    regs.set_sp(address);
    Some(address)
}

fn trap_reports_next_instruction(arch: Arch) -> bool {
    matches!(arch, Arch::X86 | Arch::X86_64)
}

fn unskip_entry_instruction(pid: Pid, tracee: &Tracee) {
    if !trap_reports_next_instruction(tracee.arch) {
        return;
    }
    let Ok(mut regs) = read_registers(pid, tracee.arch) else {
        return;
    };
    if regs.pc() != tracee.entry.wrapping_add(1) {
        return;
    }
    regs.set_pc(tracee.entry);
    write_registers(pid, &regs);
}

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

fn resolve_symbol(pid: Pid, basename: &str, symbol: &str) -> Option<usize> {
    let (base, path) = find_library(pid, basename)?;
    let address = elf::symbol_address(Path::new(&path), base, symbol);
    (address != 0).then_some(address)
}

fn resolve_dlopen_ext(pid: Pid) -> Option<usize> {
    let symbols = [
        ("linker64", "__loader_android_dlopen_ext"),
        ("linker", "__loader_android_dlopen_ext"),
        ("libdl.so", "android_dlopen_ext"),
    ];
    symbols
        .iter()
        .find_map(|(library, symbol)| resolve_symbol(pid, library, symbol))
}

fn resolve_dlerror(pid: Pid) -> Option<usize> {
    resolve_symbol(pid, "libdl.so", "dlerror")
}

fn resolve_syscall(pid: Pid) -> Option<usize> {
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

pub(crate) fn release_tracee(pid: Pid, tracee: &Tracee) {
    restore_entry(pid, tracee);
    if tracee.state == State::Entry {
        unskip_entry_instruction(pid, tracee);
    } else {
        write_registers(pid, &tracee.saved);
    }
}
