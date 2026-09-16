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

use std::ffi::{CStr, CString, c_char};

use nix::sys::utsname::uname;

/// Read a bionic system property. The property area is specific to Android's C
/// library, so no crate models it.
pub fn system_property(key: &str) -> Option<String> {
    let key = CString::new(key).ok()?;
    let mut value = [0 as c_char; libc::PROP_VALUE_MAX as usize];
    let length = unsafe { libc::__system_property_get(key.as_ptr(), value.as_mut_ptr()) };
    if length <= 0 {
        return None;
    }
    Some(
        unsafe { CStr::from_ptr(value.as_ptr()) }
            .to_string_lossy()
            .into_owned(),
    )
}

pub fn kernel_release() -> Option<String> {
    let name = uname().ok()?;
    Some(name.release().to_string_lossy().into_owned())
}
