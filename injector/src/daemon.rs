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

use std::collections::{BTreeMap, HashMap, HashSet};
use std::fs;
use std::io;
use std::os::unix::net::UnixListener;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;

use crate::companion;
use crate::config;
use crate::log::{loge, logi, logw};
use crate::mode::Candidate;
use crate::procfs;
use crate::state::StateWriter;
use crate::sys::fs as raw_fs;
use crate::sys::wait;
use crate::sys::{Pid, ptrace, signal};
use crate::system::{self, SystemInfo};
use crate::targets::{ModuleInfo, Target};
use crate::trace::Tracee;

pub const MODULE_ID: &str = "zygisknextsu";
pub const VERSION: &str = env!("ZNN_VERSION");

static RESCAN: AtomicBool = AtomicBool::new(false);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tracking {
    Ptrace,
    Proc,
}

impl Tracking {
    pub fn as_str(self) -> &'static str {
        match self {
            Tracking::Ptrace => "ptrace",
            Tracking::Proc => "proc",
        }
    }
}

#[derive(Debug)]
pub struct Daemon {
    pub targets: Vec<Target>,
    pub modules: BTreeMap<String, ModuleInfo>,
    pub system: SystemInfo,
    pub loader64: PathBuf,
    pub loader32: PathBuf,
    pub module_dir: PathBuf,
    pub state: StateWriter,
    pub mode: Tracking,
    pub requested_mode: String,
    pub tracees: HashMap<Pid, Tracee>,
    pub done: HashSet<Pid>,
    pub ignored: HashSet<Pid>,
    pub candidates: HashMap<Pid, Candidate>,
    pub ignored_rescans: u32,
    pub listener: Option<UnixListener>,
    pub last_rescan_ms: Option<u64>,
    pub last_zygisk_check_ms: u64,
    pub last_state_write_ms: u64,
}

impl Daemon {
    pub fn new(module_dir: PathBuf) -> Self {
        let mut daemon = Self {
            targets: Vec::new(),
            modules: BTreeMap::new(),
            system: system::collect(),
            loader64: module_dir.join("lib64").join("libloader.so"),
            loader32: module_dir.join("lib").join("libloader.so"),
            module_dir,
            state: StateWriter::default(),
            mode: Tracking::Ptrace,
            requested_mode: config::MODE_AUTO.to_owned(),
            tracees: HashMap::new(),
            done: HashSet::new(),
            ignored: HashSet::new(),
            candidates: HashMap::new(),
            ignored_rescans: 0,
            listener: None,
            last_rescan_ms: None,
            last_zygisk_check_ms: 0,
            last_state_write_ms: 0,
        };

        logi!("Zygisk Next Next {VERSION} starting");

        daemon.check_loaders();
        signal::set_handler(signal::SIGHUP, on_sighup);

        daemon.collect_targets();
        logi!("collected {} zn modules", daemon.targets.len());

        daemon.listener = companion::create_listener();
        daemon.select_mode();
        daemon
    }

    pub fn run(&mut self) -> ! {
        self.write_state_snapshot();
        self.last_state_write_ms = procfs::now_ms();

        loop {
            if RESCAN.swap(false, Ordering::SeqCst) {
                self.collect_targets();
                self.apply_requested_mode();
                self.last_rescan_ms = Some(procfs::now_ms());
                self.state.dirty = true;
                logi!("rescanned znn targets ({})", self.targets.len());
            }

            self.accept_companion_requests();

            match self.mode {
                Tracking::Proc => self.poll_processes(),
                Tracking::Ptrace => {
                    let now = procfs::now_ms();
                    if self.requested_mode == config::MODE_AUTO
                        && now - self.last_zygisk_check_ms >= 5000
                    {
                        self.last_zygisk_check_ms = now;
                        if crate::mode::zygisk_present() {
                            logw!(
                                "Zygisk implementation started while tracing init; yielding init and switching to proc mode (poll /proc)"
                            );
                            ptrace::detach(1, None);
                            self.tracees.remove(&1);
                            self.mode = Tracking::Proc;
                            self.state.dirty = true;
                        }
                    }
                }
            }

            self.reap_traces();
            self.expire_pending();

            let now = procfs::now_ms();
            if self
                .last_rescan_ms
                .is_none_or(|last_rescan| now - last_rescan >= 5000)
            {
                self.collect_targets();
                self.last_rescan_ms = Some(now);
                self.state.dirty = true;
            }
            if self.state.dirty
                && (now - self.last_state_write_ms >= 2000 || self.last_state_write_ms == 0)
            {
                self.write_state_snapshot();
                self.state.dirty = false;
                self.last_state_write_ms = now;
            }

            thread::sleep(match self.mode {
                Tracking::Proc => Duration::from_millis(2),
                Tracking::Ptrace => Duration::from_millis(1),
            });
        }
    }

    fn check_loaders(&mut self) {
        let mut unreadable = 0;
        for path in [&self.loader64, &self.loader32] {
            if !raw_fs::access(path, raw_fs::R_OK) {
                match fs::metadata(path) {
                    Ok(_) => loge!("loader {} not readable", path.display()),
                    Err(error) => loge!("loader {} not readable: {error}", path.display()),
                }
                unreadable += 1;
                continue;
            }
            match crate::sys::fs::lgetxattr(path, "security.selinux") {
                Ok(label) => logi!(
                    "loader {} label {}",
                    path.display(),
                    String::from_utf8_lossy(&label)
                ),
                Err(error) => loge!(
                    "loader {}: cannot read selinux label: {error}",
                    path.display()
                ),
            }
        }
        if unreadable == 2 {
            self.state.status_reason = "cannot read the loader".to_owned();
        }
    }

    fn reap_traces(&mut self) {
        loop {
            match wait::waitpid(-1, wait::WALL | wait::WNOHANG) {
                Ok(Some((pid, status))) => self.handle_event(pid, status),
                Ok(None) => break,
                Err(error) => {
                    if error.kind() == io::ErrorKind::Interrupted {
                        continue;
                    }
                    if error.raw_os_error() == Some(wait::ECHILD) {
                        break;
                    }
                    loge!("waitpid failed: {error}");
                    break;
                }
            }
        }
    }
}

extern "C" fn on_sighup(_signal: i32) {
    RESCAN.store(true, Ordering::SeqCst);
}
