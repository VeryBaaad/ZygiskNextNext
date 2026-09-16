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
use std::os::fd::{IntoRawFd, RawFd};

use nix::sys::memfd::{MFdFlags, memfd_create as nix_memfd_create};

pub const MFD_CLOEXEC: i32 = MFdFlags::MFD_CLOEXEC.bits() as i32;

pub fn memfd_create(name: &str, flags: i32) -> io::Result<RawFd> {
    let flags = MFdFlags::from_bits_truncate(flags as libc::c_uint);
    nix_memfd_create(name, flags)
        .map(IntoRawFd::into_raw_fd)
        .map_err(io::Error::from)
}
