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

use std::ffi::c_void;
use std::io;

use nix::sys::ptrace::{self, Event, Options};
use nix::unistd::Pid as NixPid;

use super::process::Pid;
use super::signal;

const NT_PRSTATUS: usize = 1;

const PTRACE_SEIZE: libc::c_int = 0x4206;
const PTRACE_INTERRUPT: libc::c_int = 0x4207;

pub fn peek_data(pid: Pid, address: usize) -> io::Result<usize> {
    ptrace::read(NixPid::from_raw(pid), address as *mut c_void)
        .map(|word| word as usize)
        .map_err(io::Error::from)
}

pub fn poke_data(pid: Pid, address: usize, word: usize) -> io::Result<()> {
    ptrace::write(
        NixPid::from_raw(pid),
        address as *mut c_void,
        word as libc::c_long,
    )
    .map_err(io::Error::from)
}

pub fn get_regset(pid: Pid, buffer: &mut [u8]) -> io::Result<()> {
    regset(
        libc::PTRACE_GETREGSET,
        pid,
        buffer.as_mut_ptr().cast(),
        buffer.len(),
    )
}

pub fn set_regset(pid: Pid, buffer: &[u8]) -> io::Result<()> {
    regset(
        libc::PTRACE_SETREGSET,
        pid,
        buffer.as_ptr().cast_mut().cast(),
        buffer.len(),
    )
}

pub fn seize(pid: Pid, options: usize) -> io::Result<()> {
    if unsafe { libc::ptrace(PTRACE_SEIZE, pid, 0usize, options) } != 0 {
        return Err(io::Error::last_os_error());
    }
    Ok(())
}

pub fn interrupt(pid: Pid) -> io::Result<()> {
    if unsafe { libc::ptrace(PTRACE_INTERRUPT, pid, 0usize, 0usize) } != 0 {
        return Err(io::Error::last_os_error());
    }
    Ok(())
}

pub fn event_message(pid: Pid) -> io::Result<usize> {
    ptrace::getevent(NixPid::from_raw(pid))
        .map(|message| message as usize)
        .map_err(io::Error::from)
}

pub fn cont(pid: Pid, signal_number: Option<i32>) {
    let _ = ptrace::cont(
        NixPid::from_raw(pid),
        signal_number.and_then(signal::from_number),
    );
}

pub fn detach(pid: Pid, signal_number: Option<i32>) {
    let _ = detach_result(pid, signal_number);
}

pub fn detach_result(pid: Pid, signal_number: Option<i32>) -> io::Result<()> {
    ptrace::detach(
        NixPid::from_raw(pid),
        signal_number.and_then(signal::from_number),
    )
    .map_err(io::Error::from)
}

fn regset(request: libc::c_int, pid: Pid, base: *mut c_void, length: usize) -> io::Result<()> {
    let mut iov = libc::iovec {
        iov_base: base,
        iov_len: length,
    };
    let result = unsafe {
        libc::ptrace(
            request,
            pid,
            NT_PRSTATUS,
            &mut iov as *mut libc::iovec as usize,
        )
    };
    if result != 0 {
        return Err(io::Error::last_os_error());
    }
    Ok(())
}

/// `PTRACE_EVENT_FORK`
pub const EVENT_FORK: i32 = Event::PTRACE_EVENT_FORK as i32;
/// `PTRACE_EVENT_VFORK`
pub const EVENT_VFORK: i32 = Event::PTRACE_EVENT_VFORK as i32;
/// `PTRACE_EVENT_CLONE`
pub const EVENT_CLONE: i32 = Event::PTRACE_EVENT_CLONE as i32;
/// `PTRACE_EVENT_EXEC`
pub const EVENT_EXEC: i32 = Event::PTRACE_EVENT_EXEC as i32;
/// `PTRACE_EVENT_EXIT`
pub const EVENT_EXIT: i32 = Event::PTRACE_EVENT_EXIT as i32;
/// `PTRACE_EVENT_STOP`
pub const EVENT_STOP: i32 = Event::PTRACE_EVENT_STOP as i32;

/// `PTRACE_O_TRACEFORK`
pub const OPTION_TRACEFORK: usize = Options::PTRACE_O_TRACEFORK.bits() as usize;
/// `PTRACE_O_TRACEVFORK`
pub const OPTION_TRACEVFORK: usize = Options::PTRACE_O_TRACEVFORK.bits() as usize;
/// `PTRACE_O_TRACECLONE`
pub const OPTION_TRACECLONE: usize = Options::PTRACE_O_TRACECLONE.bits() as usize;
/// `PTRACE_O_TRACEEXEC`
pub const OPTION_TRACEEXEC: usize = Options::PTRACE_O_TRACEEXEC.bits() as usize;
/// `PTRACE_O_TRACEEXIT`
pub const OPTION_TRACEEXIT: usize = Options::PTRACE_O_TRACEEXIT.bits() as usize;

pub const TRACING_OPTIONS: usize =
    OPTION_TRACEFORK | OPTION_TRACEVFORK | OPTION_TRACECLONE | OPTION_TRACEEXEC | OPTION_TRACEEXIT;
