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

#![deny(unsafe_code)]
mod arch;
mod companion;
mod config;
mod ctl;
mod daemon;
mod elf;
mod fileio;
mod live;
mod log;
mod maps;
mod mode;
mod paths;
mod procfs;
mod ptrace_ops;
mod state;
mod sys;
mod system;
mod targets;
mod trace;

fn main() {
    let matches = ctl::command().get_matches();

    if let Some(arguments) = ctl::ctl_arguments(&matches) {
        std::process::exit(ctl::ctl_main(&arguments));
    }

    let module_dir = ctl::module_dir(&matches).unwrap_or_else(ctl::module_dir_of_self);

    daemon::Daemon::new(module_dir).run();
}
