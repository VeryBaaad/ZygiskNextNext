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

use std::ffi::CString;
use std::io;

use nix::sys::signal::kill as nix_kill;
use nix::unistd::{self, ForkResult, Pid as NixPid};

use super::signal;

pub type Pid = libc::pid_t;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Forked {
    Parent(Pid),
    Child,
}

pub fn fork() -> io::Result<Forked> {
    // SAFETY: the injector forks while it is still single-threaded, so the
    // child cannot inherit a half-locked allocator or another thread's state.
    match unsafe { unistd::fork() } {
        Ok(ForkResult::Parent { child }) => Ok(Forked::Parent(child.as_raw())),
        Ok(ForkResult::Child) => Ok(Forked::Child),
        Err(error) => Err(io::Error::from(error)),
    }
}

pub fn exit_now(code: i32) -> ! {
    unsafe { libc::_exit(code) }
}

pub fn set_process_name(name: &str) {
    let Ok(name) = CString::new(name) else {
        return;
    };
    unsafe {
        libc::prctl(libc::PR_SET_NAME, name.as_ptr(), 0, 0, 0);
    }
}

pub fn kill(pid: Pid, signal_number: i32) -> io::Result<()> {
    let Some(signal) = signal::from_number(signal_number) else {
        return Err(io::Error::from(io::ErrorKind::InvalidInput));
    };
    nix_kill(NixPid::from_raw(pid), signal).map_err(io::Error::from)
}
