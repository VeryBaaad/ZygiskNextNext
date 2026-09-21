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
use std::io::IoSliceMut;
use std::os::fd::RawFd;

use nix::cmsg_space;
use nix::sys::socket::{self, ControlMessageOwned, MsgFlags, UnixAddr};

pub fn recv_with_fd(socket: RawFd, payload: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
    let mut payload = [IoSliceMut::new(payload)];
    let mut control = cmsg_space!([RawFd; 1]);
    let message = loop {
        match socket::recvmsg::<UnixAddr>(
            socket,
            &mut payload,
            Some(&mut control),
            MsgFlags::empty(),
        ) {
            Ok(message) => break message,
            Err(nix::errno::Errno::EINTR) => continue,
            Err(error) => return Err(io::Error::from(error)),
        }
    };

    let mut descriptor = None;
    for control in message.cmsgs().map_err(io::Error::from)? {
        if let ControlMessageOwned::ScmRights(descriptors) = control {
            descriptor = descriptors.first().copied();
            break;
        }
    }
    Ok((message.bytes, descriptor))
}
