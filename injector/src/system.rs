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

use std::path::Path;
use std::process::Command;

use crate::sys::fs as raw_fs;
use crate::sys::prop;

#[derive(Debug, Default)]
pub struct RootImpl {
    pub kernel_su: String,
    pub magisk: String,
    pub apatch: String,
}

#[derive(Debug, Default)]
pub struct SystemInfo {
    pub kernel: String,
    pub sdk: i32,
    pub abi: String,
    pub abilist: String,
    pub root: RootImpl,
}

pub const fn runtime_abi() -> &'static str {
    if cfg!(target_arch = "aarch64") {
        "arm64-v8a"
    } else if cfg!(target_arch = "arm") {
        "armeabi-v7a"
    } else if cfg!(target_arch = "x86_64") {
        "x86_64"
    } else if cfg!(target_arch = "x86") {
        "x86"
    } else if cfg!(target_arch = "riscv64") {
        "riscv64"
    } else {
        ""
    }
}

pub fn collect() -> SystemInfo {
    let mut info = SystemInfo {
        kernel: prop::kernel_release().unwrap_or_default(),
        sdk: prop::system_property("ro.build.version.sdk")
            .and_then(|value| value.parse().ok())
            .unwrap_or(0),
        abi: prop::system_property("ro.product.cpu.abi").unwrap_or_default(),
        abilist: prop::system_property("ro.product.cpu.abilist").unwrap_or_default(),
        root: RootImpl::default(),
    };

    if raw_fs::access(Path::new("/data/adb/ksud"), raw_fs::X_OK) {
        info.root.kernel_su = first_line("/data/adb/ksud", "--version");
    }
    if raw_fs::access(Path::new("/data/adb/magisk/magisk"), raw_fs::X_OK) {
        info.root.magisk = first_line("/data/adb/magisk/magisk", "-v");
        if info.root.magisk.is_empty() {
            info.root.magisk = first_line("/data/adb/magisk/magisk", "-V");
        }
    }
    if raw_fs::access(Path::new("/data/adb/apd"), raw_fs::X_OK) {
        info.root.apatch = first_line("/data/adb/apd", "--version");
    }
    info
}

fn first_line(binary: &str, argument: &str) -> String {
    let Ok(output) = Command::new(binary).arg(argument).output() else {
        return String::new();
    };
    let stdout = String::from_utf8_lossy(&output.stdout);
    stdout
        .lines()
        .next()
        .unwrap_or_default()
        .trim_end_matches(['\n', '\r'])
        .to_owned()
}
