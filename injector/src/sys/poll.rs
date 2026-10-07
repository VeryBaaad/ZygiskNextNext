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
use std::os::fd::RawFd;

pub fn poll_readable(descriptors: &[RawFd], timeout_ms: i32) -> io::Result<Vec<RawFd>> {
    if descriptors.is_empty() {
        return Ok(Vec::new());
    }

    let mut poll_descriptors: Vec<libc::pollfd> = descriptors
        .iter()
        .map(|descriptor| libc::pollfd {
            fd: *descriptor,
            events: libc::POLLIN,
            revents: 0,
        })
        .collect();

    // SAFETY: the pointer and the count describe the live vector above
    let ready = unsafe {
        libc::poll(
            poll_descriptors.as_mut_ptr(),
            poll_descriptors.len() as libc::nfds_t,
            timeout_ms,
        )
    };
    if ready < 0 {
        return Err(io::Error::last_os_error());
    }

    Ok(poll_descriptors
        .iter()
        .filter(|descriptor| descriptor.revents != 0)
        .map(|descriptor| descriptor.fd)
        .collect())
}
