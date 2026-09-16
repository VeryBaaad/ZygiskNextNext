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

use std::fs::File;
use std::os::unix::fs::FileExt;
use std::path::Path;

use crate::arch::Arch;

const ELF_MAGIC: [u8; 4] = [0x7f, b'E', b'L', b'F'];
const EI_CLASS: usize = 4;
const ELFCLASS64: u8 = 2;

const ET_DYN: u16 = 3;

const EM_386: u16 = 3;
const EM_ARM: u16 = 40;
const EM_X86_64: u16 = 62;
const EM_AARCH64: u16 = 183;
const EM_RISCV: u16 = 243;

const SHT_SYMTAB: u32 = 2;
const SHT_DYNSYM: u32 = 11;
const SHN_UNDEF: u16 = 0;

#[derive(Debug, Clone, Copy)]
pub struct ElfHeader {
    pub is_64: bool,
    pub kind: u16,
    pub machine: u16,
    pub entry: u64,
    section_headers: u64,
    section_entry_size: u16,
    section_count: u16,
}

impl ElfHeader {
    pub fn is_position_independent(&self) -> bool {
        self.kind == ET_DYN
    }
}

pub fn parse_header(bytes: &[u8]) -> Option<ElfHeader> {
    if bytes.len() < 52 || bytes[..ELF_MAGIC.len()] != ELF_MAGIC {
        return None;
    }
    let is_64 = bytes[EI_CLASS] == ELFCLASS64;
    if is_64 && bytes.len() < 64 {
        return None;
    }
    Some(ElfHeader {
        is_64,
        kind: read_u16(bytes, 16)?,
        machine: read_u16(bytes, 18)?,
        entry: if is_64 {
            read_u64(bytes, 24)?
        } else {
            u64::from(read_u32(bytes, 24)?)
        },
        section_headers: if is_64 {
            read_u64(bytes, 40)?
        } else {
            u64::from(read_u32(bytes, 32)?)
        },
        section_entry_size: read_u16(bytes, if is_64 { 58 } else { 46 })?,
        section_count: read_u16(bytes, if is_64 { 60 } else { 48 })?,
    })
}

pub fn header_of_file(path: &Path) -> Option<ElfHeader> {
    let file = File::open(path).ok()?;
    let mut buffer = [0u8; 64];
    let read = file.read_at(&mut buffer, 0).ok()?;
    parse_header(&buffer[..read])
}

pub fn arch_of(header: &ElfHeader) -> Arch {
    if header.is_64 {
        match header.machine {
            EM_AARCH64 => Arch::Arm64,
            EM_X86_64 => Arch::X86_64,
            EM_RISCV => Arch::Riscv64,
            _ => Arch::Unknown,
        }
    } else {
        match header.machine {
            EM_ARM => Arch::Arm32,
            EM_386 => Arch::X86,
            _ => Arch::Unknown,
        }
    }
}

pub fn symbol_address(path: &Path, base: usize, symbol: &str) -> usize {
    let Ok(bytes) = std::fs::read(path) else {
        return 0;
    };
    let Some(header) = parse_header(&bytes) else {
        return 0;
    };
    find_symbol(&bytes, &header, base, symbol).unwrap_or(0)
}

fn find_symbol(bytes: &[u8], header: &ElfHeader, base: usize, symbol: &str) -> Option<usize> {
    if header.section_count == 0 {
        return None;
    }
    let table = usize::try_from(header.section_headers).ok()?;
    let entry_size = usize::from(header.section_entry_size);
    let count = usize::from(header.section_count);
    if table.checked_add(count.checked_mul(entry_size)?)? > bytes.len() {
        return None;
    }

    for index in 0..count {
        let section = table.checked_add(index.checked_mul(entry_size)?)?;
        let section_type = read_u32(bytes, section + 4)?;
        if section_type != SHT_DYNSYM && section_type != SHT_SYMTAB {
            continue;
        }

        let strings_index = read_u32(bytes, section + header.scalar(40, 24))? as usize;
        let entries_offset =
            usize::try_from(header.word(bytes, section + 24, section + 16)?).ok()?;
        let entries_size = usize::try_from(header.word(bytes, section + 32, section + 20)?).ok()?;
        let entry_stride = usize::try_from(header.word(bytes, section + 56, section + 36)?).ok()?;
        if strings_index >= count || entry_stride == 0 {
            continue;
        }

        let strings_header = table.checked_add(strings_index.checked_mul(entry_size)?)?;
        let strings_offset =
            usize::try_from(header.word(bytes, strings_header + 24, strings_header + 16)?).ok()?;
        let strings_size =
            usize::try_from(header.word(bytes, strings_header + 32, strings_header + 20)?).ok()?;

        for entry in 0..entries_size / entry_stride {
            let symbol_entry = entries_offset.checked_add(entry.checked_mul(entry_stride)?)?;
            let name_offset = usize::try_from(read_u32(bytes, symbol_entry)?).ok()?;
            let section_index = read_u16(bytes, symbol_entry + if header.is_64 { 6 } else { 14 })?;
            if section_index == SHN_UNDEF || name_offset >= strings_size {
                continue;
            }

            let name_at = strings_offset.checked_add(name_offset)?;
            let name = bytes.get(name_at..)?;
            let name_end = name.iter().position(|byte| *byte == 0)?;
            if name[..name_end] != *symbol.as_bytes() {
                continue;
            }

            let value = header.word(bytes, symbol_entry + 8, symbol_entry + 4)?;
            let value = usize::try_from(value).ok()?;
            return Some(if header.is_position_independent() {
                base.wrapping_add(value)
            } else {
                value
            });
        }
    }
    None
}

impl ElfHeader {
    fn word(&self, bytes: &[u8], offset_64: usize, offset_32: usize) -> Option<u64> {
        if self.is_64 {
            read_u64(bytes, offset_64)
        } else {
            Some(u64::from(read_u32(bytes, offset_32)?))
        }
    }

    fn scalar(&self, offset_64: usize, offset_32: usize) -> usize {
        if self.is_64 { offset_64 } else { offset_32 }
    }
}

fn read_u16(bytes: &[u8], offset: usize) -> Option<u16> {
    let end = offset.checked_add(2)?;
    let slice = bytes.get(offset..end)?;
    Some(u16::from_le_bytes([slice[0], slice[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Option<u32> {
    let end = offset.checked_add(4)?;
    let slice = bytes.get(offset..end)?;
    Some(u32::from_le_bytes([slice[0], slice[1], slice[2], slice[3]]))
}

fn read_u64(bytes: &[u8], offset: usize) -> Option<u64> {
    let low = u64::from(read_u32(bytes, offset)?);
    let high = u64::from(read_u32(bytes, offset.checked_add(4)?)?);
    Some(low | (high << 32))
}
