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
use std::os::fd::{BorrowedFd, FromRawFd, OwnedFd, RawFd};
use std::path::Path;

use nix::errno::Errno;
use nix::unistd::{AccessFlags, access as nix_access, write as nix_write};

pub const F_OK: i32 = AccessFlags::empty().bits();
pub const R_OK: i32 = AccessFlags::R_OK.bits();
pub const X_OK: i32 = AccessFlags::X_OK.bits();

pub fn access(path: impl AsRef<Path>, mode: i32) -> bool {
    nix_access(path.as_ref(), AccessFlags::from_bits_truncate(mode)).is_ok()
}

/// Read a file's extended attribute without following a final symlink. `nix`
/// models no xattr syscalls, so bionic provides this one directly.
pub fn lgetxattr(path: impl AsRef<Path>, name: &str) -> io::Result<Vec<u8>> {
    let path = path_cstring(path.as_ref())?;
    let name = CString::new(name).map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))?;
    // An SELinux context is a few dozen bytes at most; 256 matches what the
    // shell tools use.
    let mut buffer = vec![0u8; 256];
    let read = unsafe {
        libc::lgetxattr(
            path.as_ptr(),
            name.as_ptr(),
            buffer.as_mut_ptr().cast(),
            buffer.len(),
        )
    };
    if read < 0 {
        return Err(io::Error::last_os_error());
    }
    buffer.truncate(read as usize);
    Ok(buffer)
}

/// Write a whole buffer to a descriptor that the caller keeps owning.
pub fn write_all(fd: RawFd, mut buffer: &[u8]) -> io::Result<()> {
    // SAFETY: the descriptor stays open for the whole call and is never
    // dropped through this borrow.
    let descriptor = unsafe { BorrowedFd::borrow_raw(fd) };
    while !buffer.is_empty() {
        match nix_write(descriptor, buffer) {
            Ok(0) => return Err(io::Error::from(io::ErrorKind::WriteZero)),
            Ok(written) => buffer = &buffer[written..],
            Err(Errno::EINTR) => continue,
            Err(error) => return Err(io::Error::from(error)),
        }
    }
    Ok(())
}

/// Close a raw descriptor, ignoring the result: once the descriptor is gone a
/// `close(2)` failure cannot be retried meaningfully.
pub fn close(fd: RawFd) {
    // SAFETY: the caller hands over its only reference to `fd`, so taking
    // ownership performs exactly one `close(2)` when the value drops.
    drop(unsafe { OwnedFd::from_raw_fd(fd) });
}

fn path_cstring(path: &Path) -> io::Result<CString> {
    CString::new(path.as_os_str().as_encoded_bytes())
        .map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))
}
