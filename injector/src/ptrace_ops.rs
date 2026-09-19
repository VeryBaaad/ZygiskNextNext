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

use std::io;

use crate::arch::{Arch, Regs};
use crate::sys::Pid;
use crate::sys::ptrace;

const WORD: usize = std::mem::size_of::<usize>();
const MAX_STRING: usize = 512;

pub fn read_memory(pid: Pid, address: usize, buffer: &mut [u8]) -> bool {
    let mut done = 0;
    while done < buffer.len() {
        let current = address.wrapping_add(done);
        let aligned = current & !(WORD - 1);
        let within = current - aligned;
        let Ok(word) = ptrace::peek_data(pid, aligned) else {
            return false;
        };
        let bytes = word.to_ne_bytes();
        let take = (buffer.len() - done).min(WORD - within);
        buffer[done..done + take].copy_from_slice(&bytes[within..within + take]);
        done += take;
    }
    true
}

pub fn write_memory(pid: Pid, address: usize, buffer: &[u8]) -> bool {
    let mut done = 0;
    while done < buffer.len() {
        let current = address.wrapping_add(done);
        let aligned = current & !(WORD - 1);
        let within = current - aligned;
        let take = (buffer.len() - done).min(WORD - within);

        let mut word = 0usize;
        if within != 0 || take < WORD {
            match ptrace::peek_data(pid, aligned) {
                Ok(existing) => word = existing,
                Err(_) => return false,
            }
        }
        let mut bytes = word.to_ne_bytes();
        bytes[within..within + take].copy_from_slice(&buffer[done..done + take]);
        if ptrace::poke_data(pid, aligned, usize::from_ne_bytes(bytes)).is_err() {
            return false;
        }
        done += take;
    }
    true
}

pub fn read_registers(pid: Pid, arch: Arch) -> io::Result<Regs> {
    let mut regs = Regs::new(arch);
    ptrace::get_regset(pid, regs.bytes_mut())?;
    Ok(regs)
}

pub fn write_registers(pid: Pid, regs: &Regs) -> bool {
    ptrace::set_regset(pid, regs.bytes()).is_ok()
}

pub fn setup_call(
    pid: Pid,
    regs: &mut Regs,
    function: usize,
    return_to: usize,
    args: &[usize],
) -> bool {
    match regs.arch() {
        Arch::Arm64 | Arch::Arm32 | Arch::Riscv64 => {
            for (index, argument) in args.iter().enumerate() {
                if !regs.set_argument(index, *argument) {
                    break;
                }
            }
            regs.set_link(return_to);
            regs.set_pc(function);
            true
        }
        Arch::X86_64 => {
            for (index, argument) in args.iter().enumerate() {
                if !regs.set_argument(index, *argument) {
                    return false;
                }
            }
            let stack = regs.sp().wrapping_sub(WORD);
            if !write_memory(pid, stack, &return_to.to_ne_bytes()) {
                return false;
            }
            regs.set_sp(stack);
            regs.set_pc(function);
            true
        }
        Arch::X86 => {
            let mut stack = regs.sp();
            for argument in args.iter().rev() {
                stack = stack.wrapping_sub(4);
                if !write_memory(pid, stack, &(*argument as u32).to_ne_bytes()) {
                    return false;
                }
            }
            stack = stack.wrapping_sub(4);
            if !write_memory(pid, stack, &(return_to as u32).to_ne_bytes()) {
                return false;
            }
            regs.set_sp(stack);
            regs.set_pc(function);
            true
        }
        Arch::Unknown => false,
    }
}

pub fn read_c_string(pid: Pid, address: usize) -> String {
    let mut bytes = Vec::with_capacity(64);
    let mut byte = [0u8; 1];
    while bytes.len() < MAX_STRING {
        if !read_memory(pid, address.wrapping_add(bytes.len()), &mut byte) || byte[0] == 0 {
            break;
        }
        bytes.push(byte[0]);
    }
    String::from_utf8_lossy(&bytes).into_owned()
}
