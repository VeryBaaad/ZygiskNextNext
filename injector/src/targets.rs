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

use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};

use crate::daemon::Daemon;
use crate::paths;
use crate::procfs::{self, basename};
use crate::sys::Pid;
use crate::sys::fs as raw_fs;

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Target {
    pub by_name: bool,
    pub value: String,
}

impl Target {
    pub fn matches(&self, exe: &str) -> bool {
        if self.by_name {
            basename(exe) == self.value
        } else {
            exe == self.value
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct ModuleInfo {
    pub id: String,
    pub name: String,
    pub version: String,
    pub targets: Vec<Target>,
    pub declarations: Vec<Declaration>,
    pub processes: Vec<(Pid, String)>,
    pub failed: Vec<(String, String)>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Declaration {
    pub target: Target,
    pub companion: bool,
    pub library: String,
}

pub fn parse_declaration(line: &str) -> Option<Declaration> {
    let tokens: Vec<&str> = line.split_whitespace().collect();
    if tokens.len() < 2 {
        return None;
    }
    let (by_name, value) = if let Some(value) = tokens[0].strip_prefix("path=") {
        (false, value)
    } else {
        (true, tokens[0].strip_prefix("name=")?)
    };

    Some(Declaration {
        target: Target {
            by_name,
            value: value.to_owned(),
        },
        companion: tokens[1..tokens.len() - 1].contains(&"companion"),
        library: tokens[tokens.len() - 1].to_owned(),
    })
}

pub fn declarations(module_dir: &Path) -> Option<Vec<Declaration>> {
    let contents = fs::read_to_string(module_dir.join("zn_modules.txt")).ok()?;
    Some(contents.lines().filter_map(parse_declaration).collect())
}

pub fn is_enabled(module_dir: &Path) -> bool {
    !raw_fs::access(module_dir.join("disable"), raw_fs::F_OK)
        && !raw_fs::access(module_dir.join("remove"), raw_fs::F_OK)
}

pub fn read_prop(module_dir: &Path, key: &str) -> String {
    let Ok(contents) = fs::read_to_string(module_dir.join("module.prop")) else {
        return String::new();
    };
    for line in contents.lines() {
        let line = line.trim_end_matches(['\n', '\r']);
        if let Some(value) = line
            .strip_prefix(key)
            .and_then(|rest| rest.strip_prefix('='))
        {
            return value.to_owned();
        }
    }
    String::new()
}

pub fn collect(previous: &BTreeMap<String, ModuleInfo>) -> BTreeMap<String, ModuleInfo> {
    let mut modules = BTreeMap::new();
    let Ok(entries) = fs::read_dir(paths::MODULES_DIR) else {
        return modules;
    };

    for entry in entries.flatten() {
        let file_name = entry.file_name();
        let Some(id) = file_name.to_str() else {
            continue;
        };
        if id.starts_with('.') {
            continue;
        }
        let module_dir = PathBuf::from(paths::MODULES_DIR).join(id);
        if !is_enabled(&module_dir) {
            continue;
        }
        let Some(declarations) = declarations(&module_dir) else {
            continue;
        };

        let mut module = ModuleInfo {
            id: id.to_owned(),
            ..ModuleInfo::default()
        };
        module.name = read_prop(&module_dir, "name");
        if module.name.is_empty() {
            module.name = module.id.clone();
        }
        module.version = read_prop(&module_dir, "version");
        module.declarations = declarations;
        module.targets = module
            .declarations
            .iter()
            .map(|declaration| declaration.target.clone())
            .collect();
        if module.targets.is_empty() {
            continue;
        }

        if let Some(previous) = previous.get(id) {
            module.processes = previous.processes.clone();
            module.processes.retain(|(pid, _)| procfs::is_alive(*pid));
            module.failed = previous.failed.clone();
        }
        modules.insert(module.id.clone(), module);
    }
    modules
}

pub fn flatten(modules: &BTreeMap<String, ModuleInfo>) -> Vec<Target> {
    let mut seen = BTreeSet::new();
    let mut targets = Vec::new();
    for module in modules.values() {
        for target in &module.targets {
            if seen.insert(target.clone()) {
                targets.push(target.clone());
            }
        }
    }
    targets
}

pub fn matches(targets: &[Target], exe: &str) -> bool {
    targets.iter().any(|target| target.matches(exe))
}

pub fn module_matches(module: &ModuleInfo, exe: &str) -> bool {
    !exe.is_empty() && matches(&module.targets, exe)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ModuleLibrary {
    pub path: String,
    pub companion: bool,
}

pub fn resolve_library(module_dir: &Path, library: &str) -> Option<String> {
    let candidate = if library.starts_with('/') {
        PathBuf::from(library)
    } else {
        module_dir.join(library)
    };
    let library = fs::canonicalize(candidate).ok()?;
    let module = fs::canonicalize(module_dir).ok()?;
    if library == module || !library.starts_with(&module) {
        return None;
    }
    Some(library.to_string_lossy().into_owned())
}

pub fn libraries_for(modules: &BTreeMap<String, ModuleInfo>, exe: &str) -> Vec<ModuleLibrary> {
    let mut libraries = Vec::new();
    let mut seen = BTreeSet::new();
    for module in modules.values() {
        let module_dir = PathBuf::from(paths::MODULES_DIR).join(&module.id);
        for declaration in &module.declarations {
            if !declaration.target.matches(exe) {
                continue;
            }
            let Some(path) = resolve_library(&module_dir, &declaration.library) else {
                continue;
            };
            if !seen.insert(path.clone()) {
                continue;
            }
            libraries.push(ModuleLibrary {
                path,
                companion: declaration.companion,
            });
        }
    }
    libraries
}

impl Daemon {
    pub fn collect_targets(&mut self) {
        self.modules = collect(&self.modules);
        self.targets = flatten(&self.modules);
    }

    pub fn record_success(&mut self, pid: Pid, exe: &str) {
        let name = procfs::process_name(pid, exe);
        let mut changed = false;
        for module in self.modules.values_mut() {
            if !module_matches(module, exe) {
                continue;
            }
            if module.processes.iter().any(|(known, _)| *known == pid) {
                continue;
            }
            module.processes.push((pid, name.clone()));
            changed = true;
        }
        if changed {
            self.state.dirty = true;
        }
    }

    pub fn record_failure(&mut self, pid: Pid, exe: &str, reason: &str) {
        let name = procfs::process_name(pid, exe);
        let mut changed = false;
        for module in self.modules.values_mut() {
            if !module_matches(module, exe) {
                continue;
            }
            if module
                .failed
                .iter()
                .any(|(failed_name, failed_reason)| *failed_name == name && failed_reason == reason)
            {
                continue;
            }
            module.failed.push((name.clone(), reason.to_owned()));
            changed = true;
        }
        if changed {
            self.state.dirty = true;
        }
    }
}
