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
use std::path::Path;

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

/// Start `executable` in a session of its own with stdio on /dev/null, so the
/// daemon outlives the control client that asked for it. The returned pid is
/// the daemon's: `execv` keeps it.
pub fn spawn_detached(executable: &Path, argument: &Path) -> io::Result<Pid> {
    match fork()? {
        Forked::Parent(pid) => Ok(pid),
        Forked::Child => {
            // SAFETY: the child only performs async-signal-safe calls before
            // exec, and leaves through `_exit` when exec cannot happen.
            unsafe {
                libc::setsid();
                let devnull = libc::open(c"/dev/null".as_ptr(), libc::O_RDWR);
                if devnull >= 0 {
                    libc::dup2(devnull, libc::STDIN_FILENO);
                    libc::dup2(devnull, libc::STDOUT_FILENO);
                    libc::dup2(devnull, libc::STDERR_FILENO);
                    if devnull > libc::STDERR_FILENO {
                        libc::close(devnull);
                    }
                }
                let Some(path) = path_cstring(executable) else {
                    exit_now(1)
                };
                let Some(argument) = path_cstring(argument) else {
                    exit_now(1)
                };
                let argv = [path.as_ptr(), argument.as_ptr(), std::ptr::null()];
                libc::execv(path.as_ptr(), argv.as_ptr());
            }
            exit_now(1)
        }
    }
}

fn path_cstring(path: &Path) -> Option<CString> {
    CString::new(path.as_os_str().as_encoded_bytes()).ok()
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
