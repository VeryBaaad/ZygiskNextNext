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

use std::fs;
use std::io::{Read, Write};
use std::os::fd::{AsRawFd, RawFd};
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::{Path, PathBuf};
use std::time::Duration;

use crate::config;
use crate::daemon::Daemon;
use crate::log::{loge, logi};
use crate::paths;
use crate::procfs;
use crate::sys;
use crate::targets;

/// ZNNC
const REQUEST_MAGIC: u32 = 0x5A4E_4E43;
const COMMAND_SPAWN: u32 = 1;
const COMMAND_MODULES: u32 = 2;
const COMMAND_CONFIG: u32 = 3;
const CONNECT: u8 = 1;
const MAX_FIELD: u32 = 4096;
const REQUEST_TIMEOUT: Duration = Duration::from_secs(2);

pub fn create_listener() -> Option<UnixListener> {
    let _ = fs::remove_file(paths::COMPANION_SOCKET);
    let listener = match UnixListener::bind(paths::COMPANION_SOCKET) {
        Ok(listener) => listener,
        Err(error) => {
            loge!(
                "cannot create companion socket {}: {error}",
                paths::COMPANION_SOCKET
            );
            return None;
        }
    };
    if listener.set_nonblocking(true).is_err() {
        return None;
    }
    let _ = fs::set_permissions(paths::COMPANION_SOCKET, fs::Permissions::from_mode(0o666));
    logi!("companion socket ready at {}", paths::COMPANION_SOCKET);
    Some(listener)
}

impl Daemon {
    pub fn accept_companion_requests(&self) {
        let Some(listener) = self.listener.as_ref() else {
            return;
        };
        loop {
            match listener.accept() {
                Ok((stream, _)) => {
                    let _ = stream.set_read_timeout(Some(REQUEST_TIMEOUT));
                    self.handle_companion_request(stream);
                }
                Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => break,
                Err(error) if error.kind() == std::io::ErrorKind::Interrupted => continue,
                Err(_) => break,
            }
        }
    }

    fn handle_companion_request(&self, mut stream: UnixStream) {
        let (Some(magic), Some(command)) = (read_u32(&mut stream), read_u32(&mut stream)) else {
            return;
        };
        if magic != REQUEST_MAGIC {
            return;
        }

        match command {
            COMMAND_MODULES => {
                let (Some(process_name), Some(process_path)) =
                    (read_string(&mut stream), read_string(&mut stream))
                else {
                    return;
                };
                self.send_modules_for_process(&mut stream, &process_name, &process_path);
            }
            COMMAND_CONFIG => {
                let config = config::effective();
                if !send_string(&mut stream, &config.inline_hook)
                    || !send_string(&mut stream, &config.plt_hook)
                {
                    loge!("config request: write to client failed");
                }
            }
            COMMAND_SPAWN => {
                let Some(library_path) = read_string(&mut stream) else {
                    return;
                };
                spawn_companion(&mut stream, &library_path);
            }
            _ => {}
        }
    }

    fn send_modules_for_process(
        &self,
        stream: &mut UnixStream,
        process_name: &str,
        process_path: &str,
    ) {
        if let Ok(entries) = fs::read_dir(paths::MODULES_DIR) {
            for entry in entries.flatten() {
                let file_name = entry.file_name();
                let Some(id) = file_name.to_str() else {
                    continue;
                };
                if id.starts_with('.') {
                    continue;
                }
                let module_dir = PathBuf::from(paths::MODULES_DIR).join(id);
                if !targets::is_enabled(&module_dir) {
                    continue;
                }
                let Some(declarations) = targets::declarations(&module_dir) else {
                    continue;
                };

                for declaration in declarations {
                    send_declaration(
                        stream,
                        &module_dir,
                        &declaration,
                        process_name,
                        process_path,
                    );
                }
            }
        }

        let _ = stream.write_all(&0u32.to_ne_bytes());
    }
}

fn declaration_matches(
    declaration: &targets::Declaration,
    process_name: &str,
    process_path: &str,
) -> bool {
    if declaration.target.by_name {
        declaration.target.value == process_name
    } else {
        declaration.target.value == process_path
    }
}

fn send_declaration(
    stream: &UnixStream,
    module_dir: &Path,
    declaration: &targets::Declaration,
    process_name: &str,
    process_path: &str,
) {
    if !declaration_matches(declaration, process_name, process_path) {
        return;
    }
    let Some(library) = resolve_module_library(module_dir, &declaration.library) else {
        return;
    };
    let descriptor = memfd_from_file(&library);
    send_module_record(stream, &library, declaration.companion, descriptor);
    if let Some(descriptor) = descriptor {
        sys::fs::close(descriptor);
    }
}

fn resolve_module_library(module_dir: &Path, library: &str) -> Option<String> {
    let candidate = if library.starts_with('/') {
        PathBuf::from(library)
    } else {
        module_dir.join(library)
    };
    let library = fs::canonicalize(candidate).ok()?;
    let module = fs::canonicalize(module_dir).ok()?;
    if library == module || !library.starts_with(&module) {
        loge!(
            "module library {} is outside {}",
            library.display(),
            module.display()
        );
        return None;
    }
    Some(library.to_string_lossy().into_owned())
}

fn memfd_from_file(path: &str) -> Option<RawFd> {
    let contents = procfs::read_file(Path::new(path))?;
    let descriptor = sys::memfd::memfd_create("znn-module", sys::memfd::MFD_CLOEXEC).ok()?;
    if sys::fs::write_all(descriptor, &contents).is_err() {
        sys::fs::close(descriptor);
        return None;
    }
    Some(descriptor)
}

fn send_module_record(
    stream: &UnixStream,
    library: &str,
    companion: bool,
    descriptor: Option<RawFd>,
) {
    let length = library.len() as u32 + 1;
    let mut payload = Vec::with_capacity(4 + library.len() + 1 + 4);
    payload.extend_from_slice(&length.to_ne_bytes());
    payload.extend_from_slice(library.as_bytes());
    payload.push(0);
    payload.extend_from_slice(&u32::from(companion).to_ne_bytes());
    let _ = sys::socket::send_with_fd(stream.as_raw_fd(), &payload, descriptor);
}

fn send_string(stream: &mut UnixStream, value: &str) -> bool {
    let length = value.len() as u32 + 1;
    let mut payload = Vec::with_capacity(4 + value.len() + 1);
    payload.extend_from_slice(&length.to_ne_bytes());
    payload.extend_from_slice(value.as_bytes());
    payload.push(0);
    stream.write_all(&payload).is_ok()
}

fn read_u32(stream: &mut UnixStream) -> Option<u32> {
    let mut buffer = [0u8; 4];
    stream.read_exact(&mut buffer).ok()?;
    Some(u32::from_ne_bytes(buffer))
}

fn read_string(stream: &mut UnixStream) -> Option<String> {
    let length = read_u32(stream)?;
    if length == 0 || length > MAX_FIELD {
        return None;
    }
    let mut buffer = vec![0u8; length as usize];
    stream.read_exact(&mut buffer).ok()?;
    if buffer.pop() != Some(0) {
        return None;
    }
    Some(String::from_utf8_lossy(&buffer).into_owned())
}

fn spawn_companion(stream: &mut UnixStream, library_path: &str) {
    let Ok((loader_end, companion_end)) = UnixStream::pair() else {
        return;
    };

    match sys::process::fork() {
        Ok(sys::process::Forked::Child) => {
            sys::fs::close(stream.as_raw_fd());
            drop(loader_end);
            run_companion_process(library_path, companion_end.as_raw_fd())
        }
        Ok(sys::process::Forked::Parent(pid)) => {
            drop(companion_end);
            let sent = sys::socket::send_with_fd(
                stream.as_raw_fd(),
                &REQUEST_MAGIC.to_ne_bytes(),
                Some(loader_end.as_raw_fd()),
            );
            if sent.is_ok() {
                logi!("companion spawned for {library_path} (pid {pid})");
            }
        }
        Err(error) => loge!("companion: cannot fork: {error}"),
    }
}

fn run_companion_process(library_path: &str, control: RawFd) -> ! {
    sys::process::set_process_name("znn-companion");

    let library = sys::dlfcn::Library::open(library_path).or_else(|| {
        let descriptor = memfd_from_file(library_path)?;
        sys::dlfcn::Library::open_from_fd(library_path, descriptor)
    });
    let Some(library) = library else {
        loge!(
            "companion: dlopen {library_path} failed: {}",
            sys::dlfcn::last_error()
        );
        sys::process::exit_now(1);
    };
    let Some(companion) = library.companion() else {
        loge!("companion: {library_path} does not export zn_companion_module");
        sys::process::exit_now(1);
    };

    logi!("companion: serving {library_path}");
    companion.notify_loaded();

    let mut command = [0u8; 1];
    while let Ok((received, descriptor)) = sys::socket::recv_with_fd(control, &mut command) {
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
