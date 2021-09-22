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

#include "src/stirling/source_connectors/pid_runtime/bcc_bpf_intf/pidruntime.h"

BPF_HASH(pid_cpu_time, uint16_t, struct pidruntime_val_t);

int trace_pid_runtime(struct pt_regs* ctx) {
  uint16_t cur_pid = bpf_get_current_pid_tgid() >> 32;

  struct pidruntime_val_t* cur_val = pid_cpu_time.lookup(&cur_pid);
  uint64_t cur_ts = bpf_ktime_get_ns();

  if (cur_val) {
    // Only update the timestamp and run_time.
    cur_val->run_time += cur_ts - cur_val->timestamp;
    cur_val->timestamp = cur_ts;
    return 0;
  }
  // Create a new entry.
  struct pidruntime_val_t new_val = {.timestamp = cur_ts, .run_time = 0};
  bpf_get_current_comm(&new_val.name, sizeof(new_val.name));
  pid_cpu_time.update(&cur_pid, &new_val);
  return 0;
}
