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

#[cfg(target_pointer_width = "64")]
mod implementation {
    use std::fmt;
    use std::os::fd::{AsRawFd, RawFd};

    use aya::Ebpf;
    use aya::maps::{MapData, RingBuf};
    use aya::programs::RawTracePoint;

    use crate::kernel::{EventKind, KernelEvent};
    use crate::sys::Pid;

    const OBJECT: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/exec.bpf.o"));
    const RING_BUFFER_MAP: &str = "EVENTS";
    const EXEC_PROGRAM: &str = "handle_exec";
    const EXEC_TRACEPOINT: &str = "sched_process_exec";
    const RECORD_SIZE: usize = 40;

    pub struct BpfTracker {
        _bpf: Ebpf,
        ring: RingBuf<MapData>,
    }

    impl fmt::Debug for BpfTracker {
        fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
            formatter.write_str("BpfTracker")
        }
    }

    impl BpfTracker {
        pub fn start() -> Result<Self, String> {
            let mut bpf = Ebpf::load(OBJECT)
                .map_err(|error| format!("cannot load the BPF object: {error}"))?;
            {
                let program: &mut RawTracePoint = bpf
                    .program_mut(EXEC_PROGRAM)
                    .ok_or_else(|| format!("the BPF object has no {EXEC_PROGRAM} program"))?
                    .try_into()
                    .map_err(|error| {
                        format!("{EXEC_PROGRAM} is not a raw tracepoint program: {error}")
                    })?;
                program
                    .load()
                    .map_err(|error| format!("the verifier rejected {EXEC_PROGRAM}: {error}"))?;
                program
                    .attach(EXEC_TRACEPOINT)
                    .map_err(|error| format!("cannot attach {EXEC_TRACEPOINT}: {error}"))?;
            }

            let map = bpf
                .take_map(RING_BUFFER_MAP)
                .ok_or_else(|| format!("the BPF object has no {RING_BUFFER_MAP} map"))?;
            let ring = RingBuf::try_from(map)
                .map_err(|error| format!("{RING_BUFFER_MAP} is not a ring buffer: {error}"))?;
            Ok(Self { _bpf: bpf, ring })
        }

        pub fn descriptor(&self) -> RawFd {
            self.ring.as_raw_fd()
        }

        pub fn drain(&mut self, events: &mut Vec<KernelEvent>) {
            while let Some(record) = self.ring.next() {
                let bytes: &[u8] = &record;
                if let Some(event) = decode(bytes) {
                    events.push(event);
                }
            }
        }
    }

    fn decode(record: &[u8]) -> Option<KernelEvent> {
        if record.len() < RECORD_SIZE {
            return None;
        }
        Some(KernelEvent {
            kind: EventKind::from_raw(read_u32(record, 0)?)?,
            pid: read_u32(record, 4)? as Pid,
            uid: read_u32(record, 12)?,
            ppid: read_u32(record, 16)? as Pid,
            comm: record[24..40].try_into().ok()?,
        })
    }

    fn read_u32(bytes: &[u8], offset: usize) -> Option<u32> {
        Some(u32::from_ne_bytes(
            bytes.get(offset..offset + 4)?.try_into().ok()?,
        ))
    }
}

#[cfg(not(target_pointer_width = "64"))]
mod implementation {
    use std::os::fd::RawFd;

    use crate::kernel::KernelEvent;

    #[derive(Debug)]
    pub struct BpfTracker;

    impl BpfTracker {
        pub fn start() -> Result<Self, String> {
            Err("this target is 32-bit, so only the proc connector source is available".to_owned())
        }

        pub fn descriptor(&self) -> RawFd {
            -1
        }

        pub fn drain(&mut self, _events: &mut Vec<KernelEvent>) {}
    }
}

pub use implementation::BpfTracker;
