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

use std::io::{BufRead, BufReader};
use std::path::Path;
use std::process::{Command, Stdio};
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};

use crate::sys::fs as raw_fs;
use crate::sys::prop;

/// Upper bound on how long a helper may take to answer; past it the probe is
/// abandoned so a misbehaving helper cannot hold up the daemon.
const PROBE_TIMEOUT: Duration = Duration::from_millis(500);
const HELPER_TIMEOUT: Duration = Duration::from_secs(2);

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

/// Read the first line a helper prints for the WebUI's root description. The
/// probe is bounded and never inherits our stdio: `Command::output()` waits for
/// every copy of the helper's stdout to close, so a helper that leaves a
/// process behind would stall the daemon for good.
fn first_line(binary: &str, argument: &str) -> String {
    let Ok(mut child) = Command::new(binary)
        .arg(argument)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .spawn()
    else {
        return String::new();
    };
    let Some(stdout) = child.stdout.take() else {
        let _ = child.wait();
        return String::new();
    };

    let (sender, receiver) = mpsc::channel();
    thread::spawn(move || {
        let mut line = String::new();
        let _ = BufReader::new(stdout).read_line(&mut line);
        let _ = sender.send(line);
    });
    let line = receiver.recv_timeout(PROBE_TIMEOUT).unwrap_or_default();

    if child.try_wait().ok().flatten().is_none() {
        let _ = child.kill();
    }
    let _ = child.wait();
    line.trim_end_matches(['\n', '\r']).to_owned()
}

pub fn run_helper(binary: &str, arguments: &[&str], environment: &[(&str, &str)]) -> bool {
    let mut command = Command::new(binary);
    command
        .args(arguments)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null());
    for (key, value) in environment {
        command.env(key, value);
    }
    let Ok(mut child) = command.spawn() else {
        return false;
    };

    let deadline = Instant::now() + HELPER_TIMEOUT;
    loop {
        match child.try_wait() {
            Ok(Some(status)) => return status.success(),
            Ok(None) => {}
            Err(_) => break,
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            break;
        }
        thread::sleep(Duration::from_millis(10));
    }
    let _ = child.wait();
    false
}
