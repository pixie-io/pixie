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
#include "src/stirling/source_connectors/proc_exit/bcc_bpf_intf/proc_exit.h"
#include "src/stirling/upid/upid.h"

BPF_PERF_OUTPUT(proc_exit_events);

// This array records singular values that are used by probes. We group them together to reduce the
// number of arrays with only 1 element. Use of per-cpu array shall be the most efficient way,
// compared to using hash table.
BPF_PERCPU_ARRAY(control_values, uint64_t, NUM_CONTROL_VALUES);

static __inline uint64_t read_exit_code(const struct task_struct* task) {
  int idx = TASK_STRUCT_EXIT_CODE_OFFSET_INDEX;
  int64_t* exit_code_offset = control_values.lookup(&idx);
  // If the offset were not set (uninitialized value is 0), just use the value obtained from system
  // headers.
  if (exit_code_offset == NULL || *exit_code_offset == 0) {
    return task->exit_code;
  }
  uint32_t exit_code = 0;
  bpf_probe_read(&exit_code, sizeof(exit_code), (uint8_t*)task + *exit_code_offset);
  return exit_code;
}

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
    event.exit_code = read_exit_code(task);
    bpf_get_current_comm(&event.comm, sizeof(event.comm));

    proc_exit_events.perf_submit(args, &event, sizeof(event));
  }

  return 0;
}
