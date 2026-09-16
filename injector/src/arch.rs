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

const REGS_CAPACITY: usize = 272;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Arch {
    Arm32,
    Arm64,
    X86,
    X86_64,
    Riscv64,
    Unknown,
}

struct Layout {
    size: usize,
    word: usize,
    pc: usize,
    sp: usize,
    retval: usize,
    link: Option<usize>,
    arg_registers: &'static [usize],
    memfd_create: i64,
}

const ARM64: Layout = Layout {
    size: 34 * 8,
    word: 8,
    pc: 32 * 8,
    sp: 31 * 8,
    retval: 0,
    link: Some(30 * 8),
    arg_registers: &[0, 8, 16, 24],
    memfd_create: 279,
};

const ARM32: Layout = Layout {
    size: 18 * 4,
    word: 4,
    pc: 15 * 4,
    sp: 13 * 4,
    retval: 0,
    link: Some(14 * 4),
    arg_registers: &[0, 4, 8, 12],
    memfd_create: 385,
};

const X86_64: Layout = Layout {
    size: 27 * 8,
    word: 8,
    pc: 128,
    sp: 152,
    retval: 80,
    link: None,
    // rdi, rsi, rdx, rcx
    arg_registers: &[112, 104, 96, 88],
    memfd_create: 319,
};

const X86: Layout = Layout {
    size: 17 * 4,
    word: 4,
    pc: 48,
    sp: 60,
    retval: 24,
    link: None,
    arg_registers: &[],
    memfd_create: 356,
};

const RISCV64: Layout = Layout {
    size: 32 * 8,
    word: 8,
    pc: 0,
    sp: 16,
    retval: 80,
    link: Some(8),
    // a0..a7
    arg_registers: &[80, 88, 96, 104, 112, 120, 128, 136],
    memfd_create: 279,
};

impl Arch {
    fn layout(self) -> Option<&'static Layout> {
        match self {
            Arch::Arm32 => Some(&ARM32),
            Arch::Arm64 => Some(&ARM64),
            Arch::X86 => Some(&X86),
            Arch::X86_64 => Some(&X86_64),
            Arch::Riscv64 => Some(&RISCV64),
            Arch::Unknown => None,
        }
    }

    pub fn is_64(self) -> bool {
        matches!(self, Arch::Arm64 | Arch::X86_64 | Arch::Riscv64)
    }

    pub fn register_size(self) -> usize {
        self.layout().map_or(0, |layout| layout.size)
    }

    pub fn memfd_create_number(self) -> i64 {
        self.layout().map_or(-1, |layout| layout.memfd_create)
    }

    pub fn breakpoint(self, thumb: bool) -> &'static [u8] {
        match self {
            Arch::Arm64 => &[0x00, 0x00, 0x20, 0xD4],   // brk #0
            Arch::Riscv64 => &[0x73, 0x00, 0x10, 0x00], // ebreak
            Arch::X86_64 | Arch::X86 => &[0xCC],        // int3
            Arch::Arm32 if thumb => &[0x00, 0xBE],      // bkpt #0 (Thumb)
            Arch::Arm32 => &[0x70, 0x00, 0x20, 0xE1],   // bkpt #0 (ARM)
            Arch::Unknown => &[],
        }
    }
}

#[derive(Debug, Clone, Copy)]
#[repr(C, align(8))]
pub struct Regs {
    arch: Arch,
    buffer: [u8; REGS_CAPACITY],
}

impl Regs {
    pub fn new(arch: Arch) -> Self {
        Self {
            arch,
            buffer: [0; REGS_CAPACITY],
        }
    }

    pub fn arch(&self) -> Arch {
        self.arch
    }

    pub fn bytes(&self) -> &[u8] {
        &self.buffer[..self.arch.register_size()]
    }

    pub fn bytes_mut(&mut self) -> &mut [u8] {
        let size = self.arch.register_size();
        &mut self.buffer[..size]
    }

    pub fn pc(&self) -> usize {
        self.layout().map_or(0, |layout| self.read(layout.pc))
    }

    pub fn set_pc(&mut self, value: usize) {
        if let Some(layout) = self.layout() {
            self.write(layout.pc, value);
        }
    }

    pub fn sp(&self) -> usize {
        self.layout().map_or(0, |layout| self.read(layout.sp))
    }

    pub fn set_sp(&mut self, value: usize) {
        if let Some(layout) = self.layout() {
            self.write(layout.sp, value);
        }
    }

    pub fn return_value(&self) -> usize {
        self.layout().map_or(0, |layout| self.read(layout.retval))
    }

    pub fn link(&self) -> Option<usize> {
        self.layout()?.link.map(|offset| self.read(offset))
    }

    pub fn set_link(&mut self, value: usize) {
        if let Some(offset) = self.layout().and_then(|layout| layout.link) {
            self.write(offset, value);
        }
    }

    pub fn set_argument(&mut self, index: usize, value: usize) -> bool {
        let Some(offset) = self
            .layout()
            .and_then(|layout| layout.arg_registers.get(index).copied())
        else {
            return false;
        };
        self.write(offset, value);
        true
    }

    fn layout(&self) -> Option<&'static Layout> {
        self.arch.layout()
    }

    fn read(&self, offset: usize) -> usize {
        let Some(layout) = self.layout() else {
            return 0;
        };
        let Some(end) = offset.checked_add(layout.word) else {
            return 0;
        };
        let Some(bytes) = self.buffer.get(offset..end) else {
            return 0;
        };
        if layout.word == 8 {
            let mut value = [0u8; 8];
            value.copy_from_slice(bytes);
            usize::try_from(u64::from_le_bytes(value)).unwrap_or(0)
        } else {
            let mut value = [0u8; 4];
            value.copy_from_slice(bytes);
            u32::from_le_bytes(value) as usize
        }
    }

    fn write(&mut self, offset: usize, value: usize) {
        let Some(layout) = self.layout() else {
            return;
        };
        let Some(end) = offset.checked_add(layout.word) else {
            return;
        };
        let Some(bytes) = self.buffer.get_mut(offset..end) else {
            return;
        };
        if layout.word == 8 {
            bytes.copy_from_slice(&(value as u64).to_le_bytes());
        } else {
            bytes.copy_from_slice(&(value as u32).to_le_bytes());
        }
    }
}
