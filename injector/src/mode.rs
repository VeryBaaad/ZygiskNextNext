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

use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::thread;
use std::time::Duration;

use crate::arch::Arch;
use crate::config;
use crate::daemon::{Daemon, Tracking};
use crate::elf;
use crate::log::{logi, logw};
use crate::maps;
use crate::procfs;
use crate::ptrace_ops::read_registers;
use crate::sys::Pid;
use crate::sys::ptrace;
use crate::sys::signal;
use crate::sys::wait;
use crate::trace::{State, Tracee, restore_entry, set_entry_breakpoint};

#[derive(Debug, Clone)]
pub struct Candidate {
    pub exe: String,
    pub target_since_ms: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Seizure {
    Seized,
    Retry,
    GiveUp,
}

impl Daemon {
    pub fn select_mode(&mut self) {
        self.requested_mode = config::effective().mode;
        if self.requested_mode == config::MODE_PROC {
            logi!("tracking mode forced to proc (poll /proc); init is never traced");
            self.mode = Tracking::Proc;
        } else if self.requested_mode == config::MODE_PTRACE {
            logi!("tracking mode forced to ptrace (trace init)");
            self.mode = if self.seize_init() {
                Tracking::Ptrace
            } else {
                Tracking::Proc
            };
        } else if zygisk_present() {
            logi!("Zygisk implementation detected; using proc mode (poll /proc)");
            self.mode = Tracking::Proc;
        } else {
            self.mode = if self.seize_init() {
                Tracking::Ptrace
            } else {
                Tracking::Proc
            };
        }
    }

    pub fn apply_requested_mode(&mut self) {
        let wanted = config::effective().mode;
        if wanted == self.requested_mode {
            return;
        }
        self.requested_mode = wanted.clone();

        if wanted == config::MODE_PROC && self.mode == Tracking::Ptrace {
            ptrace::detach(1, None);
            self.tracees.remove(&1);
            self.mode = Tracking::Proc;
            self.state.dirty = true;
            logw!("tracking mode changed to proc; detached init and switched to polling /proc");
        } else if wanted != config::MODE_PROC && self.mode == Tracking::Proc {
            logw!(
                "tracking mode {wanted} only applies on the next injector start; staying in proc mode"
            );
        }
    }

    fn seize_init(&mut self) -> bool {
        logi!("tracing init (pid 1)");
        if let Err(error) = ptrace::seize(1, ptrace::TRACING_OPTIONS) {
            logw!(
                "cannot seize init: {error} — another tracer holds pid 1; using proc mode (poll /proc) instead"
            );
            return false;
        }
        self.tracees
            .insert(1, Tracee::new(State::Traced, Arch::Unknown));
        logi!("successfully seized init");
        true
    }

    pub fn expire_pending(&mut self) {
        let now = procfs::now_ms();
        let expired: Vec<Pid> = self
            .tracees
            .iter()
            .filter(|(_, tracee)| {
                tracee.state == State::Entry && tracee.deadline_ms != 0 && now >= tracee.deadline_ms
            })
            .map(|(pid, _)| *pid)
            .collect();

        for pid in expired {
            logw!("entry breakpoint for pid {pid} never hit (already past entry?), detaching");
            if let Some(tracee) = self.tracees.remove(&pid) {
                restore_entry(pid, &tracee);
            }
            ptrace::detach(pid, None);
            self.done.insert(pid);
        }
    }

    pub fn poll_processes(&mut self) {
        let mut seen = std::collections::HashSet::new();
        let Ok(entries) = std::fs::read_dir("/proc") else {
            return;
        };

        for entry in entries.flatten() {
            let file_name = entry.file_name();
            let Ok(pid) = file_name.to_string_lossy().parse::<Pid>() else {
                continue;
            };
            if pid <= 1 {
                continue;
            }
            seen.insert(pid);

            if self.tracees.contains_key(&pid)
                || self.done.contains(&pid)
                || self.ignored.contains(&pid)
            {
                continue;
            }

            let exe = procfs::read_exe_path(pid);
            if exe.is_empty() {
                continue; // zombie or gone; pruned below
            }
            let is_target = crate::targets::matches(&self.targets, &exe);

            match self.candidates.get(&pid).cloned() {
                None => {
                    if is_target {
                        self.candidates.insert(
                            pid,
                            Candidate {
                                exe: exe.clone(),
                                target_since_ms: procfs::now_ms(),
                            },
                        );
                        self.observe_target(pid, &exe);
                    } else if procfs::is_pre_exec_fork(&exe) {
                        self.candidates.insert(
                            pid,
                            Candidate {
                                exe,
                                target_since_ms: 0,
                            },
                        );
                    } else {
                        self.ignored.insert(pid);
                    }
                    continue;
                }
                Some(candidate) => {
                    if exe == candidate.exe {
                        if is_target {
                            if procfs::now_ms() - candidate.target_since_ms > 5000 {
                                self.done.insert(pid);
                                self.candidates.remove(&pid);
                            } else {
                                self.observe_target(pid, &exe);
                            }
                        }
                        continue;
                    }
                    let since = if is_target { procfs::now_ms() } else { 0 };
                    if let Some(tracked) = self.candidates.get_mut(&pid) {
                        tracked.exe = exe.clone();
                        tracked.target_since_ms = since;
                    }
                    if is_target {
                        self.observe_target(pid, &exe);
                    } else {
                        self.candidates.remove(&pid);
                        self.ignored.insert(pid);
                    }
                }
            }
        }

        self.candidates.retain(|pid, _| seen.contains(pid));
        self.ignored.retain(|pid| seen.contains(pid));
        self.done.retain(|pid| seen.contains(pid));
        self.ignored_rescans += 1;
        if self.ignored_rescans >= 15000 {
            self.ignored_rescans = 0;
            self.ignored.clear();
        }
    }

    fn observe_target(&mut self, pid: Pid, exe: &str) {
        match self.seize_target(pid, exe) {
            Seizure::Retry => {}
            Seizure::Seized | Seizure::GiveUp => {
                self.done.insert(pid);
                self.candidates.remove(&pid);
            }
        }
    }

    fn seize_target(&mut self, pid: Pid, exe: &str) -> Seizure {
        let Some(header) = elf::header_of_file(Path::new(&format!("/proc/{pid}/exe"))) else {
            return Seizure::GiveUp;
        };
        let arch = elf::arch_of(&header);
        if arch == Arch::Unknown {
            logw!(
                "skipping {exe} (pid {pid}): unsupported machine {}",
                header.machine
            );
            return Seizure::GiveUp;
        }

        let mut base = 0;
        let mut has_loader = false;
        for entry in maps::parse_for_pid(pid) {
            if entry.offset == 0 && procfs::map_path_equals_exe(&entry.path, exe) {
                base = entry.start;
            }
            if entry.path.contains("libloader.so") || entry.path.contains("memfd:loader") {
                has_loader = true;
            }
        }
        if base == 0 {
            return Seizure::GiveUp;
        }
        if has_loader {
            logw!(
                "{exe} (pid {pid}) already carries the ZNN loader (fork of an injected process); skipping"
            );
            return Seizure::GiveUp;
        }

        let (entry, thumb) = crate::trace::entry_point(arch, &header, base);
        if entry == 0 {
            return Seizure::GiveUp;
        }

        if let Err(error) = ptrace::seize(pid, 0) {
            if error.kind() == std::io::ErrorKind::PermissionDenied {
                return Seizure::Retry;
            }
            return Seizure::GiveUp;
        }

        if ptrace::interrupt(pid).is_err() {
            ptrace::detach(pid, None);
            return Seizure::GiveUp;
        }

        let deadline = procfs::now_ms() + 200;
        let status = loop {
            match wait::waitpid(pid, wait::WALL | wait::WNOHANG) {
                Ok(Some((waited, status))) if waited == pid => break status,
                Ok(_) => {}
                Err(_) => {
                    ptrace::detach(pid, None);
                    return Seizure::GiveUp;
                }
            }
            if procfs::now_ms() >= deadline {
                ptrace::detach(pid, None);
                return Seizure::GiveUp;
            }
            thread::sleep(Duration::from_micros(500));
        };
        if status.is_exited() || status.is_signaled() {
            ptrace::detach(pid, None);
            return Seizure::GiveUp;
        }
        let mut resume_signal = None;
        if status.is_stopped() {
            let stopped_by = status.stop_signal();
            if stopped_by != signal::SIGTRAP {
                resume_signal = Some(stopped_by);
            }
        }

        let Ok(regs) = read_registers(pid, arch) else {
            ptrace::detach(pid, None);
            return Seizure::GiveUp;
        };
        let pc = regs.pc();
        if procfs::pc_in_exe_text(pid, exe, pc) {
            logw!("{exe} (pid {pid}) already past entry (pc {pc:#x}); instance missed");
            ptrace::detach(pid, None);
            return Seizure::GiveUp;
        }

        let mut tracee = Tracee::new(State::Entry, arch);
        tracee.entry = entry;
        tracee.exe = exe.to_owned();
        tracee.loader = if header.is_64 {
            self.loader64.to_string_lossy().into_owned()
        } else {
            self.loader32.to_string_lossy().into_owned()
        };
        tracee.deadline_ms = procfs::now_ms() + 3000;
        if !set_entry_breakpoint(pid, &mut tracee, thumb) {
            ptrace::detach(pid, None);
            return Seizure::GiveUp;
        }
        self.tracees.insert(pid, tracee);

        logi!(
            "seized {exe} (pid {pid}) mid-linker, entry breakpoint at {entry:#x} ({})",
            if header.is_64 { "64-bit" } else { "32-bit" }
        );
        ptrace::cont(pid, resume_signal);
        Seizure::Seized
    }
}

pub(crate) fn zygisk_present() -> bool {
    let Ok(entries) = std::fs::read_dir("/proc") else {
        return false;
    };
    for entry in entries.flatten() {
        let file_name = entry.file_name();
        let name = file_name.to_string_lossy();
        if !name.as_bytes().first().is_some_and(u8::is_ascii_digit) {
            continue;
        }
        let Ok(mut file) = File::open(format!("/proc/{name}/comm")) else {
            continue;
        };
        let mut comm = [0u8; 63];
        let Ok(read) = file.read(&mut comm) else {
            continue;
        };
        if comm[..read].windows(6).any(|window| window == b"zygisk") {
            return true;
        }
    }
    false
}
