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

use std::fmt::Write as _;
use std::fs;
use std::process::Command;

use serde::Serialize;

use crate::config;
use crate::daemon::{Daemon, MODULE_ID, Tracking, VERSION};
use crate::fileio;
use crate::log::loge;
use crate::paths;
use crate::sys::fs as raw_fs;
use crate::system::RootImpl;
use crate::targets::ModuleInfo;

/// The snapshot the WebUI reads through `injector --ctl`.
#[derive(Debug, Serialize)]
pub struct StateReport<'a> {
    running: bool,
    pid: u32,
    mode: &'a str,
    version: &'a str,
    system: SystemReport<'a>,
    config: config::HookConfigReport,
    modules: Vec<ModuleReport<'a>>,
}

#[derive(Debug, Serialize)]
pub struct SystemReport<'a> {
    kernel: &'a str,
    sdk: i32,
    abi: &'a str,
    abilist: &'a str,
    root: RootReport<'a>,
}

impl SystemReport<'_> {
    /// Stand-in for a device whose injector has not published any state yet.
    pub fn empty() -> SystemReport<'static> {
        SystemReport {
            kernel: "",
            sdk: 0,
            abi: "",
            abilist: "",
            root: RootReport {
                kernel_su: "",
                magisk: "",
                apatch: "",
            },
        }
    }
}

#[derive(Debug, Serialize)]
struct RootReport<'a> {
    #[serde(rename = "kernelSU")]
    kernel_su: &'a str,
    magisk: &'a str,
    apatch: &'a str,
}

#[derive(Debug, Serialize)]
struct ModuleReport<'a> {
    id: &'a str,
    name: &'a str,
    version: &'a str,
    processes: Vec<ProcessReport<'a>>,
    failed: Vec<FailedReport<'a>>,
}

#[derive(Debug, Serialize)]
struct ProcessReport<'a> {
    pid: u32,
    name: &'a str,
}

#[derive(Debug, Serialize)]
struct FailedReport<'a> {
    name: &'a str,
    reason: &'a str,
}

#[derive(Debug, Default)]
pub struct StateWriter {
    pub dirty: bool,
    last_json: String,
    pub status_reason: String,
    last_prop_text: String,
}

impl Daemon {
    pub fn state_report(&self) -> StateReport<'_> {
        StateReport {
            running: true,
            pid: std::process::id(),
            mode: self.mode.as_str(),
            version: VERSION,
            system: SystemReport {
                kernel: &self.system.kernel,
                sdk: self.system.sdk,
                abi: &self.system.abi,
                abilist: &self.system.abilist,
                root: RootReport {
                    kernel_su: &self.system.root.kernel_su,
                    magisk: &self.system.root.magisk,
                    apatch: &self.system.root.apatch,
                },
            },
            config: config::effective().report(),
            modules: self.modules.values().map(module_report).collect::<Vec<_>>(),
        }
    }

    pub fn write_state_snapshot(&mut self) {
        let report = self.state_report();
        let json = match serde_json::to_string(&report) {
            Ok(json) => json,
            Err(error) => {
                loge!("cannot encode the state snapshot: {error}");
                return;
            }
        };
        if json == self.state.last_json {
            return;
        }

        let _ = fs::create_dir_all(paths::STATE_DIR);
        if fileio::replace_file(paths::STATE_FILE.as_ref(), json.as_bytes()).is_ok() {
            self.state.last_json = json;
            self.apply_prop_text();
        }
    }

    pub fn apply_prop_text(&mut self) {
        if self.module_dir.as_os_str().is_empty() {
            return;
        }
        let text = self.prop_status_text();
        if text == self.state.last_prop_text {
            return;
        }

        let mut applied = false;
        if raw_fs::access("/data/adb/ksud", raw_fs::X_OK) {
            applied = set_prop_override("/data/adb/ksud", &text);
        }
        if !applied && raw_fs::access("/data/adb/apd", raw_fs::X_OK) {
            applied = set_prop_override("/data/adb/apd", &text);
        }
        if !applied {
            applied = self.write_prop_file(&text);
        }
        if applied {
            self.state.last_prop_text = text;
        }
    }

    fn prop_status_text(&self) -> String {
        let base = self.base_prop_description();
        let text = if !self.state.status_reason.is_empty() {
            format!("[❌ {}]", self.state.status_reason)
        } else {
            let mut status = String::from("✅injector");
            let (implementation, version) = root_implementation(&self.system.root);
            if !implementation.is_empty() {
                let _ = write!(status, ", Root: ✅{implementation} ({version})");
            }
            if self.mode == Tracking::Proc {
                status.push_str(", PL");
            }
            let _ = write!(status, ", {} module(s) loaded", self.modules.len());
            format!("[{status}]")
        };
        if base.is_empty() {
            text
        } else {
            format!("{text} {base}")
        }
    }

    fn base_prop_description(&self) -> String {
        let mut path = self.module_dir.join("module.prop.orig");
        if !raw_fs::access(&path, raw_fs::R_OK) {
            path = self.module_dir.join("module.prop");
        }
        let Ok(contents) = fs::read_to_string(&path) else {
            return String::new();
        };
        for line in contents.lines() {
            let line = line.trim_end_matches(['\n', '\r']);
            if line.len() > 12 && line.starts_with("description=") {
                return line[12..].to_owned();
            }
        }
        String::new()
    }

    fn write_prop_file(&self, text: &str) -> bool {
        let path = self.module_dir.join("module.prop");
        let Ok(contents) = fs::read_to_string(&path) else {
            return false;
        };

        let mut out = String::with_capacity(contents.len() + text.len());
        let mut replaced = false;
        for line in contents.lines() {
            let mut line = line.trim_end_matches(['\n', '\r']).to_owned();
            if !replaced && line.starts_with("description=") {
                line = format!("description={text}");
                replaced = true;
            }
            out.push_str(&line);
            out.push('\n');
        }
        if !replaced {
            return false;
        }
        fileio::replace_file(&path, out.as_bytes()).is_ok()
    }
}

fn module_report(module: &ModuleInfo) -> ModuleReport<'_> {
    ModuleReport {
        id: &module.id,
        name: &module.name,
        version: &module.version,
        processes: module
            .processes
            .iter()
            .map(|(pid, name)| ProcessReport {
                pid: *pid as u32,
                name,
            })
            .collect(),
        failed: module
            .failed
            .iter()
            .map(|(name, reason)| FailedReport { name, reason })
            .collect(),
    }
}

fn root_implementation(root: &RootImpl) -> (&str, &str) {
    if !root.kernel_su.is_empty() {
        ("KernelSU", root.kernel_su.as_str())
    } else if !root.magisk.is_empty() {
        ("Magisk", root.magisk.as_str())
    } else if !root.apatch.is_empty() {
        ("APatch", root.apatch.as_str())
    } else {
        ("", "")
    }
}

fn set_prop_override(binary: &str, text: &str) -> bool {
    Command::new(binary)
        .args([
            "module",
            "config",
            "set",
            "override.description",
            text,
            "--temp",
        ])
        .env("KSU_MODULE", MODULE_ID)
        .env("AP_MODULE", MODULE_ID)
        .status()
        .is_ok_and(|status| status.success())
}
