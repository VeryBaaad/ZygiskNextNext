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

use std::io::Write;
#[cfg(target_os = "android")]
use std::os::android::net::SocketAddrExt;
use std::os::fd::{AsRawFd, RawFd};
#[cfg(not(target_os = "android"))]
use std::os::linux::net::SocketAddrExt;
use std::os::unix::net::{SocketAddr, UnixStream};
use std::path::Path;
use std::time::{Duration, Instant};

use crate::log::{loge, logi};
use crate::plan;
use crate::procfs;
use crate::sys;
use crate::sys::process::Forked;

const CONNECT: u8 = 1;
const CONNECT_TIMEOUT: Duration = Duration::from_secs(5);
const RETRY_INTERVAL: Duration = Duration::from_millis(10);

pub fn spawn(library: &str, nonce: u32, index: u32) {
    match sys::process::fork() {
        Ok(Forked::Child) => run_helper(library, nonce, index),
        Ok(Forked::Parent(pid)) => {
            logi!("companion for {library} (module {index}) started as pid {pid}");
        }
        Err(error) => loge!("companion for {library}: cannot fork: {error}"),
    }
}

fn connect_channel(nonce: u32, index: u32) -> Option<UnixStream> {
    let name = plan::channel_name(nonce, index);
    let address = SocketAddr::from_abstract_name(name.as_bytes()).ok()?;
    let deadline = Instant::now() + CONNECT_TIMEOUT;

    loop {
        match UnixStream::connect_addr(&address) {
            Ok(stream) => return Some(stream),
            Err(error) => {
                if Instant::now() >= deadline {
                    loge!("companion: cannot reach {name}: {error}");
                    return None;
                }
                std::thread::sleep(RETRY_INTERVAL);
            }
        }
    }
}

fn memfd_from_file(path: &str) -> Option<RawFd> {
    let contents = procfs::read_file(Path::new(path))?;
    let descriptor = sys::memfd::memfd_create("znn-companion", sys::memfd::MFD_CLOEXEC).ok()?;
    if sys::fs::write_all(descriptor, &contents).is_err() {
        sys::fs::close(descriptor);
        return None;
    }
    Some(descriptor)
}

fn run_helper(library: &str, nonce: u32, index: u32) -> ! {
    sys::process::set_process_name("znn-companion");

    let module = sys::dlfcn::Library::open(library).or_else(|| {
        let descriptor = memfd_from_file(library)?;
        sys::dlfcn::Library::open_from_fd(library, descriptor)
    });
    let Some(module) = module else {
        loge!(
            "companion: dlopen {library} failed: {}",
            sys::dlfcn::last_error()
        );
        sys::process::exit_now(1);
    };
    let Some(companion) = module.companion() else {
        loge!("companion: {library} does not export zn_companion_module");
        sys::process::exit_now(1);
    };

    logi!("companion: serving {library}");
    companion.notify_loaded();

    let Some(mut channel) = connect_channel(nonce, index) else {
        sys::process::exit_now(1);
    };
    if channel.write_all(&nonce.to_ne_bytes()).is_err() {
        loge!("companion: cannot send the channel nonce");
        sys::process::exit_now(1);
    }

    let mut command = [0u8; 1];
    while let Ok((received, descriptor)) =
        sys::socket::recv_with_fd(channel.as_raw_fd(), &mut command)
    {
        if received == 0 {
            break;
        }
        if command[0] != CONNECT {
            if let Some(descriptor) = descriptor {
                sys::fs::close(descriptor);
            }
            continue;
        }
        if let Some(descriptor) = descriptor {
            companion.notify_connected(descriptor);
        }
    }
    sys::process::exit_now(0)
}
