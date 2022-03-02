/*
 * This code runs using bpf in the Linux kernel.
 * Copyright 2018- The Pixie Authors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#include "src/stirling/source_connectors/proc_exit/bcc_bpf_intf/proc_exit.h"

BPF_PERF_OUTPUT(proc_exit_events);

// A probe for the sched:sched_process_exit tracepoint.
// This probe is primarily intended for performing BPF map clean-up after a process terminates.
TRACEPOINT_PROBE(sched, sched_process_exit) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;
  uint32_t tid = id;

  bool is_thread_group_leader = tgid == tid;
  if (is_thread_group_leader) {
    struct proc_exit_event_t event = {};
    struct task_struct* task = (struct task_struct*)bpf_get_current_task();

    event.timestamp_ns = bpf_ktime_get_ns();
    event.upid.tgid = tgid;
    event.upid.start_time_ticks = read_start_boottime(task);
    event.exit_code = task->exit_code;
    bpf_get_current_comm(&event.comm, sizeof(event.comm));

    proc_exit_events.perf_submit(args, &event, sizeof(event));
  }

  return 0;
}
