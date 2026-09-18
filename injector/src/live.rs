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

//! Injection into a process that is already running.
//!
//! The entry breakpoint the proc-mode tracker uses is only a way to stop a
//! process at a known point before anything has run: the injection itself is
//! "make the target call into the linker, then into the loader". When another
//! loader consumed that point first, the same chain still works on the running
//! process, provided nothing else can execute while the loader installs its
//! hooks. Parking the whole thread group buys exactly that, and every thread is
//! put back where it was afterwards, including the one that made the calls.

use std::path::Path;
use std::thread;
use std::time::{Duration, Instant};

use crate::arch::{Arch, Regs};
use crate::daemon::Daemon;
use crate::elf;
use crate::log::{loge, logi, logw};
use crate::procfs;
use crate::ptrace_ops::{read_registers, setup_call, write_memory, write_registers};
use crate::sys::wait::Status;
use crate::sys::{Pid, ptrace, signal, wait};
use crate::trace;

/// How long one remote call may take before the attempt is abandoned.
const CALL_TIMEOUT: Duration = Duration::from_millis(500);

/// Upper bound for the whole chain, so a stuck target cannot hold the daemon.
const TOTAL_TIMEOUT: Duration = Duration::from_millis(4000);

/// How long a thread gets to react to a seize or an interrupt.
const STOP_TIMEOUT: Duration = Duration::from_millis(200);

/// Wait between polls while a remote call runs. Short enough to notice a return
/// almost immediately, long enough not to hammer the kernel.
const POLL: Duration = Duration::from_micros(250);

/// Reason reported when a tracer appeared between the caller's check and the
/// seize. The poll loop treats it as "try again", so it has to be exact.
pub const TRACER_HELD: &str = "another tracer holds the target";

/// Threads we parked. Detaching on drop is what guarantees the target is never
/// left stopped, whatever path the chain takes.
struct Parked {
    tids: Vec<Pid>,
}

impl Drop for Parked {
    fn drop(&mut self) {
        for tid in &self.tids {
            ptrace::detach(*tid, None);
        }
    }
}

/// Inject the loader into `pid`, which is already past its entry point.
pub fn inject_into_running(daemon: &Daemon, pid: Pid, exe: &str) -> Result<(), String> {
    let header = elf::header_of_file(Path::new(&format!("/proc/{pid}/exe")))
        .ok_or_else(|| "cannot read the target's ELF header".to_owned())?;
    let arch = elf::arch_of(&header);
    if arch == Arch::Unknown {
        return Err(format!("unsupported machine {}", header.machine));
    }
    let loader = if header.is_64 {
        &daemon.loader64
    } else {
        &daemon.loader32
    };
    let loader = loader.to_string_lossy().into_owned();
    let content =
        procfs::read_file(Path::new(&loader)).ok_or_else(|| "cannot read the loader".to_owned())?;

    let tids = procfs::thread_ids(pid);
    if tids.is_empty() {
        return Err("the target has no threads".to_owned());
    }
    // The thread that performs the calls must be in user space: hijacking one
    // that sits inside a syscall would corrupt the call it is making.
    let caller = tids
        .iter()
        .copied()
        .find(|tid| procfs::in_syscall(*tid) == Some(false))
        .ok_or_else(|| "every thread is inside a syscall".to_owned())?;

    let parked = park(&tids)?;
    let saved = read_registers(caller, arch)
        .map_err(|error| format!("cannot read the caller's registers: {error}"))?;
    let deadline = Instant::now() + TOTAL_TIMEOUT;

    let result = run_chain(pid, caller, arch, &saved, &loader, &content, deadline);
    // The registers may only be put back once the caller is stopped and back at
    // the point it was interrupted at: the chain leaves it that way when it
    // succeeds, and a call that is still in flight must be allowed to finish.
    // It always returns to that same point either way, because that is the
    // return address the calls were given.
    if result.is_ok() && !write_registers(caller, &saved) {
        loge!("late injection: cannot restore the registers of pid {pid}");
    }
    match &result {
        Ok(()) => logi!("loader injected into the running {exe} (pid {pid})"),
        Err(reason) => logw!("late injection into {exe} (pid {pid}) abandoned: {reason}"),
    }
    // Detaching resumes whatever is still in flight, so no thread is ever left
    // stopped, and an abandoned call finishes on its own.
    drop(parked);
    result
}

/// Park every thread of the target, or give up without touching it.
fn park(tids: &[Pid]) -> Result<Parked, String> {
    let mut parked = Parked { tids: Vec::new() };
    for tid in tids {
        if ptrace::seize(*tid, 0).is_err() {
            return Err(TRACER_HELD.to_owned());
        }
        parked.tids.push(*tid);
        if ptrace::interrupt(*tid).is_err() {
            return Err(format!("cannot interrupt thread {tid}"));
        }
    }
    for tid in parked.tids.clone() {
        if wait_for_stop(tid)?.is_none() {
            return Err(format!("thread {tid} never stopped"));
        }
    }
    Ok(parked)
}

/// Wait for one stop of `tid`. `Ok(None)` means the thread is gone.
fn wait_for_stop(tid: Pid) -> Result<Option<Status>, String> {
    let deadline = Instant::now() + STOP_TIMEOUT;
    loop {
        match wait::waitpid(tid, wait::WALL | wait::WNOHANG) {
            Ok(Some((waited, status))) if waited == tid => {
                if status.is_exited() || status.is_signaled() {
                    return Ok(None);
                }
                return Ok(Some(status));
            }
            Ok(_) => {}
            Err(error) => return Err(format!("waitpid on thread {tid} failed: {error}")),
        }
        if Instant::now() >= deadline {
            return Err(format!("thread {tid} did not stop in time"));
        }
        thread::sleep(POLL);
    }
}

/// Write the call's arguments below the stack pointer and report the address of
/// each blob together with the stack pointer to use. Lowering the stack pointer
/// past them keeps the callee from overwriting its own arguments.
fn stage(pid: Pid, saved: &Regs, blobs: &[&[u8]]) -> Result<(Vec<usize>, usize), String> {
    let size: usize = blobs.iter().map(|blob| (blob.len() + 7) & !7).sum();
    let mut address = saved.sp().wrapping_sub(size + 0x10) & !0xF;
    let stack = address;
    let mut addresses = Vec::with_capacity(blobs.len());
    for blob in blobs {
        if !write_memory(pid, address, blob) {
            return Err("cannot write the call's arguments".to_owned());
        }
        addresses.push(address);
        address += (blob.len() + 7) & !7;
    }
    Ok((addresses, stack))
}

/// Make the parked caller perform one call and wait for it to return.
fn remote_call(
    caller: Pid,
    arch: Arch,
    saved: &Regs,
    stack: usize,
    function: usize,
    arguments: &[usize],
    deadline: Instant,
) -> Result<usize, String> {
    let mut registers = *saved;
    registers.set_sp(stack);
    // The call returns to the address the thread was interrupted at, which is
    // also how its completion is recognised.
    if !setup_call(caller, &mut registers, function, saved.pc(), arguments) {
        return Err("cannot set up the remote call".to_owned());
    }
    if !write_registers(caller, &registers) {
        return Err("cannot write the caller's registers".to_owned());
    }
    ptrace::cont(caller, None);
    wait_for_return(caller, arch, saved.pc(), deadline)
}

/// Wait until the caller is back at `expected_pc`, then report the value the
/// call returned in the first result register, leaving the caller stopped.
/// Interrupts are used instead of a planted breakpoint: the target's own memory
/// and stack stay untouched.
fn wait_for_return(
    caller: Pid,
    arch: Arch,
    expected_pc: usize,
    deadline: Instant,
) -> Result<usize, String> {
    // One call may not eat the whole budget for the chain.
    let deadline = deadline.min(Instant::now() + CALL_TIMEOUT);
    loop {
        if Instant::now() >= deadline {
            return Err("the call did not return in time".to_owned());
        }
        thread::sleep(POLL);
        if ptrace::interrupt(caller).is_err() {
            return Err("cannot interrupt the caller".to_owned());
        }
        let status = wait_for_stop(caller)?
            .ok_or_else(|| "the caller disappeared during a call".to_owned())?;
        let registers = read_registers(caller, arch)
            .map_err(|error| format!("cannot read the caller's registers: {error}"))?;
        let signal = status.stop_signal();
        if registers.pc() == expected_pc {
            // A signal that raced with the return is not handled here; the
            // attempt is abandoned and the signal stays pending for the target
            // to receive once it runs again.
            if signal != signal::SIGTRAP {
                return Err(format!("signal {signal} interrupted the call"));
            }
            return Ok(registers.return_value());
        }
        // Any other stop is forwarded before the call is resumed, so the target
        // never loses a signal over this.
        ptrace::cont(caller, (signal != signal::SIGTRAP).then_some(signal));
    }
}

/// The linker's message for the last failed dlopen, for the report.
fn dlerror_text(pid: Pid, caller: Pid, arch: Arch, saved: &Regs, deadline: Instant) -> String {
    let Some(address) = trace::resolve_dlerror(pid) else {
        return "(cannot resolve dlerror)".to_owned();
    };
    let Ok((_, stack)) = stage(pid, saved, &[]) else {
        return "(cannot stage dlerror)".to_owned();
    };
    match remote_call(caller, arch, saved, stack, address, &[], deadline) {
        Ok(pointer) if pointer != 0 => crate::ptrace_ops::read_c_string(pid, pointer),
        Ok(_) => "(no error reported)".to_owned(),
        Err(reason) => format!("({reason})"),
    }
}

#[allow(clippy::too_many_arguments)]
fn run_chain(
    pid: Pid,
    caller: Pid,
    arch: Arch,
    saved: &Regs,
    loader: &str,
    content: &[u8],
    deadline: Instant,
) -> Result<(), String> {
    let syscall_address =
        trace::resolve_syscall(pid).ok_or_else(|| "cannot resolve syscall".to_owned())?;
    let (addresses, stack) = stage(pid, saved, &[b"loader\0"])?;
    let memfd = remote_call(
        caller,
        arch,
        saved,
        stack,
        syscall_address,
        &[arch.memfd_create_number() as usize, addresses[0], 0],
        deadline,
    )? as i32;
    if memfd < 0 {
        return Err(format!("memfd_create failed ({memfd})"));
    }
    if !procfs::write_to_target_fd(pid, memfd, content) {
        return Err("cannot write the loader into the target's memfd".to_owned());
    }

    let dlopen_address = trace::resolve_dlopen_ext(pid)
        .ok_or_else(|| "cannot resolve android_dlopen_ext".to_owned())?;
    let mut extinfo = [0u8; 48];
    extinfo[..8].copy_from_slice(&trace::ANDROID_DLEXT_USE_LIBRARY_FD.to_ne_bytes());
    let fd_offset = if arch.is_64() { 28 } else { 20 };
    extinfo[fd_offset..fd_offset + 4].copy_from_slice(&memfd.to_ne_bytes());
    let (addresses, stack) = stage(pid, saved, &[b"libloader.so\0", &extinfo])?;
    let handle = remote_call(
        caller,
        arch,
        saved,
        stack,
        dlopen_address,
        &[addresses[0], trace::RTLD_NOW, addresses[1], saved.pc()],
        deadline,
    )?;
    if handle == 0 {
        let error = dlerror_text(pid, caller, arch, saved, deadline);
        return Err(format!("dlopen({loader}) failed: {error}"));
    }
    logi!("loader mapped into the running pid {pid} (handle {handle:#x})");

    let dlsym_address = trace::resolve_symbol(pid, "libdl.so", "dlsym")
        .ok_or_else(|| "cannot resolve dlsym".to_owned())?;
    let (addresses, stack) = stage(pid, saved, &[b"znn_loader_init\0"])?;
    let init = remote_call(
        caller,
        arch,
        saved,
        stack,
        dlsym_address,
        &[handle, addresses[0]],
        deadline,
    )?;
    if init == 0 {
        return Err("dlsym(znn_loader_init) failed".to_owned());
    }

    let (_, stack) = stage(pid, saved, &[])?;
    remote_call(caller, arch, saved, stack, init, &[], deadline)?;
    Ok(())
}
