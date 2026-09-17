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
use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::thread;
use std::time::Duration;

use clap::builder::ValueParser;
use clap::{Arg, ArgMatches, Command as ClapCommand};
use serde::de::DeserializeOwned;
use serde::{Deserialize, Deserializer, Serialize};
use serde_json::Value;
use serde_json::value::RawValue;

use crate::config;
use crate::daemon::VERSION;
use crate::paths;
use crate::procfs;
use crate::state::SystemReport;
use crate::sys::Pid;
use crate::sys::process;
use crate::sys::signal;

/// Name of the argument that carries the control command, which may consume up
/// to three values: the command itself plus the two `config-set` operands.
const CTL_ARGUMENT: &str = "ctl";

const CTL_VALUE_NAME: &str = "COMMAND";
const MODULE_DIR_ARGUMENT: &str = "module-dir";

/// How long `--ctl restart` waits for a running daemon to go away before it
/// escalates, and after that before it reports a failure.
const STOP_INTERVAL: Duration = Duration::from_millis(25);
const STOP_ATTEMPTS: u32 = 80;

/// How long `--ctl start` waits for the daemon it forked to be observable.
const START_ATTEMPTS: u32 = 20;

const USAGE: &str = "usage: injector --ctl <status|system|modules|config|config-set <inline|plt|mode> <value>|start|restart|rescan|exit>";

/// The command line of the injector: either a module directory to serve, or a
/// control command answered on the spot.
pub fn command() -> ClapCommand {
    ClapCommand::new("injector")
        .about("Serves the Zygisk Next module API, or answers a control command.")
        .version(VERSION)
        .arg(
            Arg::new(MODULE_DIR_ARGUMENT)
                .value_name("MODULE_DIR")
                .value_parser(ValueParser::path_buf())
                .help("Installed module directory; defaults to the parent of this binary."),
        )
        .arg(
            Arg::new(CTL_ARGUMENT)
                .long(CTL_ARGUMENT)
                .value_name(CTL_VALUE_NAME)
                .num_args(1..=3)
                .conflicts_with(MODULE_DIR_ARGUMENT)
                .help(
                    "Control command: status, system, modules, config, \
                       config-set <inline|plt|mode> <value>, start, restart, rescan or exit.",
                ),
        )
}

/// The module directory to serve, when the caller named one.
pub fn module_dir(matches: &ArgMatches) -> Option<PathBuf> {
    matches.get_one::<PathBuf>(MODULE_DIR_ARGUMENT).cloned()
}

/// The control command and its operands, when `--ctl` was given.
pub fn ctl_arguments(matches: &ArgMatches) -> Option<Vec<String>> {
    let arguments = matches.get_many::<String>(CTL_ARGUMENT)?;
    Some(arguments.cloned().collect())
}

pub fn ctl_main(arguments: &[String]) -> i32 {
    let Some((command, arguments)) = arguments.split_first() else {
        return usage();
    };

    match command.as_str() {
        "rescan" => signal_daemon(signal::SIGHUP, |pid| {
            format!("injector ({pid}) requested to rescan modules")
        }),
        "exit" => signal_daemon(signal::SIGTERM, |pid| format!("injector ({pid}) exited")),
        "start" => start_daemon(false),
        "restart" => start_daemon(true),
        "config" => print_json(&config::effective().report()),
        "config-set" => config_set(arguments),
        "status" => status(state_snapshot().as_ref()),
        "system" => system(state_snapshot().as_ref()),
        "modules" => modules(state_snapshot().as_ref()),
        _ => usage(),
    }
}

/// Echo the part of the snapshot the WebUI renders as the device information.
fn system(snapshot: Option<&StateSnapshot>) -> i32 {
    match snapshot.and_then(|state| state.system.as_deref()) {
        Some(system) => print_json(system),
        None => print_json(&SystemReport::empty()),
    }
}

/// Echo the module list of the snapshot, empty while no daemon published one.
fn modules(snapshot: Option<&StateSnapshot>) -> i32 {
    match snapshot.and_then(|state| state.modules.as_deref()) {
        Some(modules) => print_json(modules),
        None => print_json(&Vec::<Value>::new()),
    }
}

/// The snapshot the daemon last published. Only the fields the control channel
/// reads are decoded; `system` and `modules` are kept as raw text so that they
/// are handed back exactly as they were written.
#[derive(Debug, Deserialize)]
struct StateSnapshot {
    #[serde(default, deserialize_with = "lenient")]
    pid: Option<Pid>,
    #[serde(default, deserialize_with = "lenient")]
    mode: Option<String>,
    #[serde(default)]
    system: Option<Box<RawValue>>,
    #[serde(default)]
    modules: Option<Box<RawValue>>,
}

/// Read a scalar the way the WebUI does: a value that arrived as a JSON string
/// must not turn a malformed snapshot into a hard failure.
fn lenient<'de, D, T>(deserializer: D) -> Result<Option<T>, D::Error>
where
    D: Deserializer<'de>,
    T: DeserializeOwned + FromStr,
{
    let Ok(value) = Option::<Value>::deserialize(deserializer) else {
        return Ok(None);
    };
    Ok(match value {
        None | Some(Value::Null) => None,
        Some(Value::String(text)) => text.parse().ok(),
        Some(value) => T::deserialize(value).ok(),
    })
}

fn state_snapshot() -> Option<StateSnapshot> {
    let contents = fs::read_to_string(paths::STATE_FILE).ok()?;
    serde_json::from_str(&contents).ok()
}

/// The running daemon: the pid the daemon published wins, and the scan only
/// covers the case of a state file that is missing or stale.
fn daemon_pid() -> Option<Pid> {
    procfs::find_daemon_pid(state_snapshot().and_then(|state| state.pid))
}

/// Bring the daemon up from this process, which is how the WebUI recovers after
/// a userspace restart that skipped the module's boot scripts. The child gets
/// its own session and stdio on /dev/null, so it outlives the caller.
fn start_daemon(restart: bool) -> i32 {
    if let Some(pid) = daemon_pid() {
        if !restart {
            return print_json(&StatusReport {
                running: true,
                pid,
                mode: "unknown",
            });
        }
        let _ = process::kill(pid, signal::SIGTERM);
        for _ in 0..STOP_ATTEMPTS {
            if !procfs::is_daemon_alive(pid) {
                break;
            }
            thread::sleep(STOP_INTERVAL);
        }
        if procfs::is_daemon_alive(pid) {
            let _ = process::kill(pid, signal::SIGKILL);
            for _ in 0..STOP_ATTEMPTS {
                if !procfs::is_daemon_alive(pid) {
                    break;
                }
                thread::sleep(STOP_INTERVAL);
            }
        }
        if procfs::is_daemon_alive(pid) {
            eprintln!("injector ({pid}) did not stop");
            return 1;
        }
    }

    let Ok(executable) = fs::read_link("/proc/self/exe") else {
        eprintln!("cannot resolve the injector path");
        return 1;
    };
    let module_dir = module_dir_of_self();
    if module_dir.as_os_str().is_empty() {
        eprintln!("cannot resolve the module directory");
        return 1;
    }
    match process::spawn_detached(&executable, &module_dir) {
        Ok(pid) => {
            // A daemon that cannot come up (bad loader, no /proc access, ...)
            // must be reported instead of being announced as running.
            for _ in 0..START_ATTEMPTS {
                if procfs::is_daemon_alive(pid) {
                    return print_json(&StatusReport {
                        running: true,
                        pid,
                        mode: "unknown",
                    });
                }
                thread::sleep(STOP_INTERVAL);
            }
            eprintln!("injector ({pid}) did not stay up");
            1
        }
        Err(error) => {
            eprintln!("cannot start the injector: {error}");
            1
        }
    }
}

/// The module directory this binary was installed into, derived from its path
/// (`<module>/bin/injector`).
pub fn module_dir_of_self() -> PathBuf {
    let Ok(executable) = fs::read_link("/proc/self/exe") else {
        return PathBuf::new();
    };
    match executable.parent().and_then(Path::parent) {
        Some(module_dir) => module_dir.to_path_buf(),
        None => PathBuf::new(),
    }
}

fn signal_daemon(requested: i32, message: fn(Pid) -> String) -> i32 {
    let Some(pid) = daemon_pid() else {
        println!("injector is not running");
        return 1;
    };
    let _ = process::kill(pid, requested);
    println!("{}", message(pid));
    0
}

fn config_set(arguments: &[String]) -> i32 {
    let [kind, value] = arguments else {
        eprintln!("usage: injector --ctl config-set <inline|plt|mode> <value>");
        return 1;
    };

    let options = match kind.as_str() {
        "inline" => config::inline_hook_options(),
        "plt" => config::plt_hook_options(),
        "mode" => config::tracking_mode_options(),
        _ => {
            eprintln!("usage: injector --ctl config-set <inline|plt|mode> <value>");
            return 1;
        }
    };
    if !config::option_in(&options, value) {
        eprintln!("invalid {kind} value \"{value}\" for this device");
        return 1;
    }

    let current = config::effective();
    let updated = config::HookConfig {
        inline_hook: if kind == "inline" {
            value.clone()
        } else {
            current.inline_hook
        },
        plt_hook: if kind == "plt" {
            value.clone()
        } else {
            current.plt_hook
        },
        mode: if kind == "mode" {
            value.clone()
        } else {
            current.mode
        },
    };
    if !config::write(&updated) {
        eprintln!("cannot write {}", paths::CONFIG_FILE);
        return 1;
    }

    if let Some(pid) = daemon_pid() {
        let _ = process::kill(pid, signal::SIGHUP);
    }

    print_json(&updated.report())
}

fn status(snapshot: Option<&StateSnapshot>) -> i32 {
    let recorded_mode = snapshot
        .and_then(|state| state.mode.as_deref())
        .filter(|mode| *mode == config::MODE_PTRACE || *mode == config::MODE_PROC);
    let pid = daemon_pid().unwrap_or(0);
    let running = pid > 0;
    let mode = match recorded_mode {
        Some(mode) if running => mode,
        _ => "unknown",
    };
    print_json(&StatusReport {
        running,
        pid: if running { pid } else { 0 },
        mode,
    })
}

#[derive(Debug, Serialize)]
struct StatusReport<'a> {
    running: bool,
    pid: Pid,
    mode: &'a str,
}

fn print_json<T: Serialize + ?Sized>(value: &T) -> i32 {
    match serde_json::to_string(value) {
        Ok(json) => {
            println!("{json}");
            0
        }
        Err(error) => {
            eprintln!("cannot encode the answer: {error}");
            1
        }
    }
}

fn usage() -> i32 {
    eprintln!("{USAGE}");
    1
}
