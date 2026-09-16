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

use crate::sys;

pub(crate) fn emit(priority: i32, message: &str) {
    sys::log::print(priority, message);
}

macro_rules! logi {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::sys::log::INFO, &format!($($arg)*))
    };
}

macro_rules! logw {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::sys::log::WARN, &format!($($arg)*))
    };
}

macro_rules! loge {
    ($($arg:tt)*) => {
        $crate::log::emit($crate::sys::log::ERROR, &format!($($arg)*))
    };
}

pub(crate) use loge;
pub(crate) use logi;
pub(crate) use logw;
