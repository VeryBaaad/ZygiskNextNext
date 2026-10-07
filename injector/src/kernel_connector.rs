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
use std::io;
use std::os::fd::RawFd;

use crate::kernel::{EventKind, KernelEvent};
use crate::log::logw;
use crate::sys::netlink::{Connector, ProcEvent};

pub struct ConnectorTracker {
    connector: Connector,
    pending: Vec<ProcEvent>,
    reported_error: bool,
}

impl fmt::Debug for ConnectorTracker {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str("ConnectorTracker")
    }
}

impl ConnectorTracker {
    pub fn start() -> io::Result<Self> {
        Ok(Self {
            connector: Connector::open()?,
            pending: Vec::new(),
            reported_error: false,
        })
    }

    pub fn descriptor(&self) -> RawFd {
        self.connector.descriptor()
    }

    pub fn drain(&mut self, events: &mut Vec<KernelEvent>) {
        self.pending.clear();
        if let Err(error) = self.connector.drain(&mut self.pending) {
            if !self.reported_error {
                self.reported_error = true;
                logw!("kernel tracking: proc connector subscription failed: {error}");
            }
            return;
        }

        for event in &self.pending {
            let (kind, pid, ppid) = match *event {
                ProcEvent::Fork { parent, child } => (EventKind::Fork, child, parent),
                ProcEvent::Exec { pid } => (EventKind::Exec, pid, 0),
                ProcEvent::Exit { pid } => (EventKind::Exit, pid, 0),
            };
            events.push(KernelEvent {
                kind,
                pid,
                uid: 0,
                ppid,
                comm: [0; 16],
            });
        }
    }
}
