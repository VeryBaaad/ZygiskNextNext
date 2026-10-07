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

use std::fmt;

use crate::daemon::Daemon;
use crate::kernel_bpf::BpfTracker;
use crate::kernel_connector::ConnectorTracker;
use crate::log::{logi, logw};
use crate::procfs;
use crate::sys::Pid;
use crate::sys::poll;

pub const POLL_TIMEOUT_MS: i32 = 20;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventKind {
    Exec,
    Fork,
    Exit,
}

impl EventKind {
    #[cfg(target_pointer_width = "64")]
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(EventKind::Exec),
            1 => Some(EventKind::Fork),
            2 => Some(EventKind::Exit),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            EventKind::Exec => "exec",
            EventKind::Fork => "fork",
            EventKind::Exit => "exit",
        }
    }
}

#[derive(Debug, Clone)]
pub struct KernelEvent {
    pub kind: EventKind,
    pub pid: Pid,
    pub uid: u32,
    pub ppid: Pid,
    pub comm: [u8; 16],
}

#[derive(Debug)]
pub struct KernelTracker {
    bpf: Option<BpfTracker>,
    connector: Option<ConnectorTracker>,
    bpf_pending: bool,
}

impl KernelTracker {
    pub fn start() -> Option<Self> {
        let connector = match ConnectorTracker::start() {
            Ok(tracker) => {
                logi!("kernel tracking: proc connector source ready (fork/exec/exit)");
                Some(tracker)
            }
            Err(error) => {
                logw!("kernel tracking: proc connector unavailable: {error}");
                None
            }
        };
        let booted =
            crate::sys::prop::system_property("sys.boot_completed").as_deref() == Some("1");
        let (bpf, bpf_pending) = if booted {
            (start_bpf(), false)
        } else {
            logi!("kernel tracking: eBPF source held back until boot completes");
            (None, true)
        };

        if bpf.is_none() && connector.is_none() {
            return None;
        }
        Some(Self {
            bpf,
            connector,
            bpf_pending,
        })
    }

    pub fn upgrade(&mut self) {
        if !self.bpf_pending {
            return;
        }
        if crate::sys::prop::system_property("sys.boot_completed").as_deref() != Some("1") {
            return;
        }
        self.bpf_pending = false;
        self.bpf = start_bpf();
    }

    pub fn description(&self) -> &'static str {
        match (
            self.bpf.is_some(),
            self.connector.is_some(),
            self.bpf_pending,
        ) {
            (true, true, _) => "proc connector fork/exec/exit events and eBPF exec events",
            (false, true, true) => {
                "proc connector fork/exec/exit events (eBPF source pending boot completion)"
            }
            (false, true, false) => "proc connector fork/exec/exit events",
            (true, false, _) => "eBPF exec events",
            (false, false, _) => "no kernel event source",
        }
    }

    pub fn poll(&mut self, events: &mut Vec<KernelEvent>) {
        let mut descriptors = Vec::new();
        if let Some(bpf) = &self.bpf {
            descriptors.push(bpf.descriptor());
        }
        if let Some(connector) = &self.connector {
            descriptors.push(connector.descriptor());
        }
        let _ = poll::poll_readable(&descriptors, POLL_TIMEOUT_MS);

        if let Some(bpf) = &mut self.bpf {
            bpf.drain(events);
        }
        if let Some(connector) = &mut self.connector {
            connector.drain(events);
        }
    }
}

fn start_bpf() -> Option<BpfTracker> {
    match BpfTracker::start() {
        Ok(tracker) => {
            logi!("kernel tracking: eBPF exec source ready (raw tracepoint sched_process_exec)");
            Some(tracker)
        }
        Err(error) => {
            logw!("kernel tracking: eBPF source unavailable: {error}");
            None
        }
    }
}

impl Daemon {
    pub fn start_kernel_tracker(&mut self) -> bool {
        if self.kernel.is_some() {
            return true;
        }
        let Some(tracker) = KernelTracker::start() else {
            return false;
        };
        logi!("tracking mode kernel: {}", tracker.description());
        self.kernel = Some(tracker);
        true
    }

    pub fn stop_kernel_tracker(&mut self) {
        if self.kernel.take().is_some() {
            logi!("kernel tracking: stopped");
        }
    }

    pub fn poll_kernel(&mut self) {
        let mut events = Vec::new();
        if let Some(tracker) = self.kernel.as_mut() {
            tracker.poll(&mut events);
        }
        for event in events {
            self.consider_kernel_event(event);
        }
    }

    fn consider_kernel_event(&mut self, event: KernelEvent) {
        let pid = event.pid;
        if pid <= 1 {
            return;
        }

        match event.kind {
            EventKind::Fork => return,
            EventKind::Exit => {
                self.candidates.remove(&pid);
                self.done.remove(&pid);
                self.ignored.remove(&pid);
                return;
            }
            EventKind::Exec => {}
        }

        if self.tracees.contains_key(&pid)
            || self.done.contains(&pid)
            || self.ignored.contains(&pid)
        {
            return;
        }

        let exe = procfs::read_exe_path(pid);
        if exe.is_empty() || !crate::targets::matches(&self.targets, &exe) {
            self.ignored.insert(pid);
            return;
        }

        let comm = String::from_utf8_lossy(&event.comm);
        logi!(
            "{exe} (pid {pid}, uid {}, ppid {}, comm {}) appeared via a {} event",
            event.uid,
            event.ppid,
            comm.trim_end_matches('\0'),
            event.kind.as_str()
        );
        self.observe_target(pid, &exe);
    }

    pub fn upgrade_kernel_sources(&mut self) {
        if let Some(tracker) = self.kernel.as_mut() {
            tracker.upgrade();
        }
    }

    pub fn prune_kernel_bookkeeping(&mut self) {
        self.done.retain(|pid| procfs::is_alive(*pid));
        self.ignored.retain(|pid| procfs::is_alive(*pid));
        self.candidates.retain(|pid, _| procfs::is_alive(*pid));
    }
}

impl fmt::Display for EventKind {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.as_str())
    }
}
