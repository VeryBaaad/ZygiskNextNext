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

use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::marker::PhantomData;
use std::os::fd::RawFd;

const ANDROID_DLEXT_USE_LIBRARY_FD: u64 = 0x10;

#[repr(C)]
struct AndroidDlextinfo {
    flags: u64,
    reserved_addr: *mut c_void,
    reserved_size: usize,
    relro_fd: c_int,
    library_fd: c_int,
    library_fd_offset: i64,
    library_namespace: *mut c_void,
}

// SAFETY: export the android_dlopen_ext
unsafe extern "C" {
    fn android_dlopen_ext(
        filename: *const c_char,
        flag: c_int,
        extinfo: *const AndroidDlextinfo,
    ) -> *mut c_void;
}

pub struct Library {
    handle: *mut c_void,
}

impl Library {
    pub fn open(path: &str) -> Option<Self> {
        let path = CString::new(path).ok()?;
        let handle = unsafe { libc::dlopen(path.as_ptr(), libc::RTLD_NOW) };
        Self::from_handle(handle)
    }

    pub fn open_from_fd(path: &str, fd: RawFd) -> Option<Self> {
        let path = CString::new(path).ok()?;
        let extinfo = AndroidDlextinfo {
            flags: ANDROID_DLEXT_USE_LIBRARY_FD,
            reserved_addr: std::ptr::null_mut(),
            reserved_size: 0,
            relro_fd: -1,
            library_fd: fd,
            library_fd_offset: 0,
            library_namespace: std::ptr::null_mut(),
        };
        let handle = unsafe { android_dlopen_ext(path.as_ptr(), libc::RTLD_NOW, &extinfo) };
        Self::from_handle(handle)
    }

    pub fn companion(&self) -> Option<Companion<'_>> {
        let symbol = unsafe { libc::dlsym(self.handle, c"zn_companion_module".as_ptr()) };
        if symbol.is_null() {
            return None;
        }
        let module = unsafe { &*symbol.cast::<CompanionModule>() };
        let companion = Companion {
            module,
            _library: PhantomData,
        };
        if companion.module.on_companion_loaded.is_none()
            || companion.module.on_module_connected.is_none()
        {
            return None;
        }
        Some(companion)
    }

    fn from_handle(handle: *mut c_void) -> Option<Self> {
        if handle.is_null() {
            None
        } else {
            Some(Self { handle })
        }
    }
}

#[repr(C)]
struct CompanionModule {
    #[allow(dead_code)]
    target_api_version: c_int,
    on_companion_loaded: Option<unsafe extern "C" fn()>,
    on_module_connected: Option<unsafe extern "C" fn(c_int)>,
}

pub struct Companion<'a> {
    module: &'a CompanionModule,
    _library: PhantomData<&'a Library>,
}

impl Companion<'_> {
    pub fn notify_loaded(&self) {
        if let Some(callback) = self.module.on_companion_loaded {
            unsafe { callback() };
        }
    }

    pub fn notify_connected(&self, fd: RawFd) {
        if let Some(callback) = self.module.on_module_connected {
            unsafe { callback(fd) };
        }
    }
}

pub fn last_error() -> String {
    let error = unsafe { libc::dlerror() };
    if error.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(error) }
        .to_string_lossy()
        .into_owned()
}
