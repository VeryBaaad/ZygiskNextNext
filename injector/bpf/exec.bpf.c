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
 * Copyright (C) 2026 VeryBaaad <verybaad@outlook.com>
 */

typedef unsigned int __u32;
typedef unsigned long long __u64;

#define SEC(NAME) __attribute__((section(NAME), used))
#define __uint(NAME, VAL) int (*NAME)[VAL]
#define always_inline __attribute__((always_inline))

#define EVENT_EXEC 0

struct {
    __uint(type, 27); /* BPF_MAP_TYPE_RINGBUF */
    __uint(max_entries, 262144);
} EVENTS SEC(".maps");

struct bpf_raw_tracepoint_args {
    __u64 args[0];
} __attribute__((preserve_access_index));

struct task_struct {
    int pid;
    struct task_struct *real_parent;
} __attribute__((preserve_access_index));

struct kernel_event {
    __u32 kind;
    __u32 pid;
    __u32 tgid;
    __u32 uid;
    __u32 ppid;
    __u32 pad;
    char comm[16];
};

static __u64 (*bpf_get_current_pid_tgid)(void) = (void *)14;
static __u64 (*bpf_get_current_uid_gid)(void) = (void *)15;
static long (*bpf_get_current_comm)(void *buf, __u32 size) = (void *)16;
static struct task_struct *(*bpf_get_current_task_btf)(void) = (void *)158;
static void *(*bpf_ringbuf_reserve)(void *map, __u64 size, __u64 flags) = (void *)131;
static void (*bpf_ringbuf_submit)(void *data, __u64 flags) = (void *)132;

SEC("raw_tracepoint/sched_process_exec")
int handle_exec(struct bpf_raw_tracepoint_args *ctx) {
    (void)ctx;
    struct kernel_event *event = bpf_ringbuf_reserve(&EVENTS, sizeof(struct kernel_event), 0);
    if (!event)
        return 0;

    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u64 uid_gid = bpf_get_current_uid_gid();
    event->kind = EVENT_EXEC;
    event->tgid = (__u32)pid_tgid;
    event->pid = (__u32)(pid_tgid >> 32);
    event->uid = (__u32)uid_gid;
    event->ppid = bpf_get_current_task_btf()->real_parent->pid;
    bpf_get_current_comm(event->comm, sizeof(event->comm));

    bpf_ringbuf_submit(event, 0);
    return 0;
}
