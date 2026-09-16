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

use serde::Serialize;

use crate::paths;
use crate::system::runtime_abi;

pub const MODE_AUTO: &str = "auto";
/// Trace init and follow every fork.
pub const MODE_PTRACE: &str = "ptrace";
/// Poll `/proc` and never trace init.
pub const MODE_PROC: &str = "proc";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HookConfig {
    pub inline_hook: String,
    pub plt_hook: String,
    pub mode: String,
}

#[derive(Debug, Serialize)]
pub struct HookOption {
    value: String,
    options: Vec<&'static str>,
}

#[derive(Debug, Serialize)]
pub struct HookConfigReport {
    #[serde(rename = "inlineHook")]
    inline_hook: HookOption,
    #[serde(rename = "pltHook")]
    plt_hook: HookOption,
    mode: HookOption,
}

pub fn inline_hook_options() -> Vec<&'static str> {
    match runtime_abi() {
        "arm64-v8a" | "armeabi-v7a" => vec!["dobby", "shadowhook"],
        "riscv64" => vec!["rv64hook"],
        _ => vec!["dobby"],
    }
}

pub fn plt_hook_options() -> Vec<&'static str> {
    if runtime_abi() == "riscv64" {
        vec!["lsplt"]
    } else {
        vec!["lsplt", "bytehook", "xhook"]
    }
}

pub fn tracking_mode_options() -> Vec<&'static str> {
    vec![MODE_AUTO, MODE_PTRACE, MODE_PROC]
}

pub fn option_in(options: &[&str], value: &str) -> bool {
    options.contains(&value)
}

impl HookConfig {
    pub fn report(&self) -> HookConfigReport {
        HookConfigReport {
            inline_hook: HookOption {
                value: self.inline_hook.clone(),
                options: inline_hook_options(),
            },
            plt_hook: HookOption {
                value: self.plt_hook.clone(),
                options: plt_hook_options(),
            },
            mode: HookOption {
                value: self.mode.clone(),
                options: tracking_mode_options(),
            },
        }
    }
}

pub fn effective() -> HookConfig {
    let inline_options = inline_hook_options();
    let plt_options = plt_hook_options();
    let mode_options = tracking_mode_options();
    let mut config = HookConfig {
        inline_hook: inline_options
            .first()
            .copied()
            .unwrap_or_default()
            .to_owned(),
        plt_hook: plt_options.first().copied().unwrap_or_default().to_owned(),
        mode: MODE_AUTO.to_owned(),
    };

    if let Some((inline_hook, plt_hook, mode)) = read_file() {
        if option_in(&inline_options, &inline_hook) {
            config.inline_hook = inline_hook;
        }
        if option_in(&plt_options, &plt_hook) {
            config.plt_hook = plt_hook;
        }
        if option_in(&mode_options, &mode) {
            config.mode = mode;
        }
    }
    config
}

pub fn write(config: &HookConfig) -> bool {
    let _ = fs::create_dir_all(paths::STATE_DIR);
    let contents = format!(
        "inline_hook={}\nplt_hook={}\nmode={}\n",
        config.inline_hook, config.plt_hook, config.mode
    );
    crate::fileio::replace_file(paths::CONFIG_FILE.as_ref(), contents.as_bytes()).is_ok()
}

fn read_file() -> Option<(String, String, String)> {
    let contents = fs::read_to_string(paths::CONFIG_FILE).ok()?;
    let mut inline_hook = String::new();
    let mut plt_hook = String::new();
    let mut mode = String::new();

    for line in contents.lines() {
        let line = line.trim_end_matches(['\n', '\r']);
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        if key.is_empty() {
            continue;
        }
        match key {
            "inline_hook" => inline_hook = value.to_owned(),
            "plt_hook" => plt_hook = value.to_owned(),
            "mode" => mode = value.to_owned(),
            _ => {}
        }
    }
    Some((inline_hook, plt_hook, mode))
}
