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

use std::ffi::{CString, c_char, c_int};

pub const INFO: c_int = 4; // info
pub const WARN: c_int = 5; // warn
pub const ERROR: c_int = 6; // error

const TAG: &std::ffi::CStr = c"ZNNinjector";
const FORMAT: &std::ffi::CStr = c"%s";

// SAFETY: export the __android_log_print
#[link(name = "log")]
unsafe extern "C" {
    fn __android_log_print(prio: c_int, tag: *const c_char, fmt: *const c_char, ...) -> c_int;
}

pub fn print(prio: c_int, message: &str) {
    let Ok(message) = CString::new(message.replace('\0', " ")) else {
        return;
    };
    unsafe {
        __android_log_print(prio, TAG.as_ptr(), FORMAT.as_ptr(), message.as_ptr());
    }
}
