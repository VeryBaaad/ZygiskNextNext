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

use std::io;

use nix::errno::Errno;
use nix::sys::wait::{self as nix_wait, WaitPidFlag, WaitStatus};
use nix::unistd::Pid as NixPid;

use super::process::Pid;

pub const WNOHANG: i32 = WaitPidFlag::WNOHANG.bits();
pub const ECHILD: i32 = Errno::ECHILD as i32;
pub const WALL: i32 = WaitPidFlag::__WALL.bits();

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Status(WaitStatus);

impl Status {
    pub fn is_exited(self) -> bool {
        matches!(self.0, WaitStatus::Exited(..))
    }

    pub fn is_signaled(self) -> bool {
        matches!(self.0, WaitStatus::Signaled(..))
    }

    pub fn is_stopped(self) -> bool {
        matches!(
            self.0,
            WaitStatus::Stopped(..) | WaitStatus::PtraceEvent(..) | WaitStatus::PtraceSyscall(..)
        )
    }

    pub fn stop_signal(self) -> i32 {
        match self.0 {
            WaitStatus::Stopped(_, signal) | WaitStatus::PtraceEvent(_, signal, _) => signal as i32,
            WaitStatus::PtraceSyscall(..) => libc::SIGTRAP,
            _ => 0,
        }
    }

    pub fn trap_event(self) -> i32 {
        match self.0 {
            WaitStatus::PtraceEvent(_, _, event) => event,
            _ => 0,
        }
    }
}

pub fn waitpid(pid: Pid, options: i32) -> io::Result<Option<(Pid, Status)>> {
    let options = WaitPidFlag::from_bits_truncate(options);
    match nix_wait::waitpid(NixPid::from_raw(pid), Some(options)) {
        Ok(WaitStatus::StillAlive) => Ok(None),
        Ok(status) => {
            let child = status.pid().map_or(pid, |child| child.as_raw());
            Ok(Some((child, Status(status))))
        }
        Err(error) => Err(io::Error::from(error)),
    }
}
