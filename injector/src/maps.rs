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

use crate::sys::Pid;

pub const PROT_READ: u8 = 0x1;
pub const PROT_WRITE: u8 = 0x2;
pub const PROT_EXEC: u8 = 0x4;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MapEntry {
    pub start: usize,
    pub end: usize,
    pub offset: usize,
    pub perms: u8,
    pub path: String,
}

impl MapEntry {
    pub fn contains(&self, address: usize) -> bool {
        address >= self.start && address < self.end
    }
}

pub fn parse_for_pid(pid: Pid) -> Vec<MapEntry> {
    parse_path(&format!("/proc/{pid}/maps"))
}

pub fn parse_path(path: &str) -> Vec<MapEntry> {
    let Ok(contents) = fs::read_to_string(path) else {
        return Vec::new();
    };
    contents.lines().filter_map(parse_line).collect()
}

fn parse_line(line: &str) -> Option<MapEntry> {
    let mut fields = line.splitn(6, ' ');
    let range = fields.next()?;
    let perms = fields.next()?;
    let offset = fields.next()?;
    fields.next()?; // device
    fields.next()?; // inode
    let path = fields.next().unwrap_or_default();

    let (start, end) = range.split_once('-')?;
    let permissions = perms.as_bytes();
    if permissions.len() < 4 {
        return None;
    }

    let mut decoded = 0;
    if permissions[0] == b'r' {
        decoded |= PROT_READ;
    }
    if permissions[1] == b'w' {
        decoded |= PROT_WRITE;
    }
    if permissions[2] == b'x' {
        decoded |= PROT_EXEC;
    }

    Some(MapEntry {
        start: usize::from_str_radix(start, 16).ok()?,
        end: usize::from_str_radix(end, 16).ok()?,
        offset: usize::from_str_radix(offset, 16).ok()?,
        perms: decoded,
        path: path.trim_end_matches(['\n', '\r']).to_owned(),
    })
}
