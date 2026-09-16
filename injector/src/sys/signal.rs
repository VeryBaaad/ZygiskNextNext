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

use nix::sys::signal::{self, SaFlags, SigAction, SigHandler, SigSet, Signal};

pub const SIGHUP: i32 = Signal::SIGHUP as i32;
pub const SIGTERM: i32 = Signal::SIGTERM as i32;
pub const SIGTRAP: i32 = Signal::SIGTRAP as i32;
pub const SIGSTOP: i32 = Signal::SIGSTOP as i32;
pub const SIGCHLD: i32 = Signal::SIGCHLD as i32;
pub const SIGSEGV: i32 = Signal::SIGSEGV as i32;
pub const SIGBUS: i32 = Signal::SIGBUS as i32;
pub const SIGABRT: i32 = Signal::SIGABRT as i32;
pub const SIGILL: i32 = Signal::SIGILL as i32;
pub const SIGFPE: i32 = Signal::SIGFPE as i32;
pub const SIGSYS: i32 = Signal::SIGSYS as i32;

pub(super) fn from_number(number: i32) -> Option<Signal> {
    Signal::try_from(number).ok()
}

pub fn set_handler(signal_number: i32, handler: extern "C" fn(i32)) {
    let Some(signal) = from_number(signal_number) else {
        return;
    };
    let action = SigAction::new(
        SigHandler::Handler(handler),
        SaFlags::empty(),
        SigSet::empty(),
    );
    // SAFETY: the handler is a plain `extern "C"` function that owns nothing,
    // and it stays installed for the remaining lifetime of the process.
    let _ = unsafe { signal::sigaction(signal, &action) };
}
