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
use std::io::Read;

pub const PLAN_MAGIC: u32 = 0x5A4E_4E31; //"ZNN1"
pub const PLAN_VERSION: u32 = 1;
pub const MODULE_COMPANION: u32 = 1 << 0;
pub const MAX_MODULES: usize = 32;
pub const MAX_BYTES: usize = 32 * 1024;
pub const HEADER_SIZE: usize = 36;
pub const MODULE_SIZE: usize = 20;

const CHANNEL_PREFIX: &str = "znn";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PlanModule {
    pub index: u32,
    pub path: String,
    pub companion: bool,
    pub fd: i32,
}

impl PlanModule {
    pub fn new(index: u32, path: String, companion: bool) -> Self {
        Self {
            index,
            path,
            companion,
            fd: -1,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Plan {
    pub modules: Vec<PlanModule>,
    pub inline_engine: String,
    pub plt_engine: String,
    pub nonce: u32,
}

fn push_string(blob: &mut Vec<u8>, value: &str) -> (u32, u32) {
    let offset = blob.len() as u32;
    blob.extend_from_slice(value.as_bytes());
    blob.push(0);
    (offset, value.len() as u32)
}

fn push_u32(out: &mut Vec<u8>, value: u32) {
    out.extend_from_slice(&value.to_ne_bytes());
}

fn push_string_ref(out: &mut Vec<u8>, reference: (u32, u32)) {
    push_u32(out, reference.0);
    push_u32(out, reference.1);
}

impl Plan {
    pub fn encode(&self) -> Option<Vec<u8>> {
        if self.modules.len() > MAX_MODULES {
            return None;
        }
        if self
            .modules
            .iter()
            .any(|module| module.index as usize >= MAX_MODULES)
        {
            return None;
        }

        let mut blob = Vec::new();
        let inline = push_string(&mut blob, &self.inline_engine);
        let plt = push_string(&mut blob, &self.plt_engine);
        let paths: Vec<(u32, u32)> = self
            .modules
            .iter()
            .map(|module| push_string(&mut blob, &module.path))
            .collect();

        let size = HEADER_SIZE + MODULE_SIZE * self.modules.len() + blob.len();
        if size > MAX_BYTES {
            return None;
        }

        let mut out = Vec::with_capacity(size);
        push_u32(&mut out, PLAN_MAGIC);
        push_u32(&mut out, PLAN_VERSION);
        push_u32(&mut out, size as u32);
        push_u32(&mut out, self.modules.len() as u32);
        push_u32(&mut out, self.nonce);
        push_string_ref(&mut out, inline);
        push_string_ref(&mut out, plt);

        for (module, path) in self.modules.iter().zip(paths) {
            let mut flags = 0u32;
            if module.companion {
                flags |= MODULE_COMPANION;
            }
            out.extend_from_slice(&module.fd.to_ne_bytes());
            push_u32(&mut out, flags);
            push_u32(&mut out, module.index);
            push_string_ref(&mut out, path);
        }
        out.extend_from_slice(&blob);

        debug_assert_eq!(out.len(), size);
        Some(out)
    }

    pub fn companions(&self) -> impl Iterator<Item = &PlanModule> {
        self.modules.iter().filter(|module| module.companion)
    }
}

pub fn channel_name(nonce: u32, index: u32) -> String {
    let mut name = String::with_capacity(32);
    let _ = write!(name, "{CHANNEL_PREFIX}.{nonce:08x}.{index}");
    name
}

pub fn fresh_nonce() -> u32 {
    if let Ok(mut urandom) = std::fs::File::open("/dev/urandom") {
        let mut bytes = [0u8; 4];
        if urandom.read_exact(&mut bytes).is_ok() {
            return u32::from_ne_bytes(bytes);
        }
    }
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map_or(0, |elapsed| elapsed.subsec_nanos());
    now ^ (std::process::id() << 8)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encode_matches_the_loader_layout() {
        let plan = Plan {
            modules: vec![
                PlanModule {
                    index: 0,
                    path: "/data/adb/modules/a/liba.so".to_owned(),
                    companion: true,
                    fd: 12,
                },
                PlanModule {
                    index: 1,
                    path: "/data/adb/modules/b/libb.so".to_owned(),
                    companion: false,
                    fd: 13,
                },
            ],
            inline_engine: "dobby".to_owned(),
            plt_engine: "lsplt".to_owned(),
            nonce: 0x1234_5678,
        };

        let bytes = plan.encode().expect("plan must fit");
        let word =
            |offset: usize| u32::from_ne_bytes(bytes[offset..offset + 4].try_into().unwrap());
        let blob = HEADER_SIZE + 2 * MODULE_SIZE;

        assert_eq!(word(0), PLAN_MAGIC);
        assert_eq!(word(4), PLAN_VERSION);
        assert_eq!(word(8) as usize, bytes.len());
        assert_eq!(word(12), 2);
        assert_eq!(word(16), 0x1234_5678);

        assert_eq!(word(20), 0);
        assert_eq!(word(24), 5);
        assert_eq!(&bytes[blob..blob + 5], b"dobby");
        assert_eq!(word(28), 6);
        assert_eq!(word(32), 5);
        assert_eq!(&bytes[blob + 6..blob + 11], b"lsplt");

        assert_eq!(word(HEADER_SIZE), 12);
        assert_eq!(word(HEADER_SIZE + 4), MODULE_COMPANION);
        assert_eq!(word(HEADER_SIZE + 8), 0);
        let offset = word(HEADER_SIZE + 12) as usize;
        let length = word(HEADER_SIZE + 16) as usize;
        assert_eq!(
            &bytes[blob + offset..blob + offset + length],
            b"/data/adb/modules/a/liba.so"
        );
        assert_eq!(bytes[blob + offset + length], 0);

        assert_eq!(word(HEADER_SIZE + MODULE_SIZE), 13);
        assert_eq!(word(HEADER_SIZE + MODULE_SIZE + 4), 0);
        assert_eq!(word(HEADER_SIZE + MODULE_SIZE + 8), 1);
    }

    #[test]
    fn oversized_plans_are_rejected() {
        let plan = Plan {
            modules: (0..MAX_MODULES + 1)
                .map(|index| PlanModule::new(index as u32, "/x".to_owned(), false))
                .collect(),
            inline_engine: String::new(),
            plt_engine: String::new(),
            nonce: 1,
        };
        assert!(plan.encode().is_none());
    }

    #[test]
    fn channel_names_match_the_loader_format() {
        assert_eq!(channel_name(0x00ab_cdef, 7), "znn.00abcdef.7");
    }
}
