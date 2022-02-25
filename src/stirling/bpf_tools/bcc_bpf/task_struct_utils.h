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

#pragma once

#include <linux/sched.h>

#include "src/stirling/bpf_tools/bcc_bpf/utils.h"

// This is how Linux converts nanoseconds to clock ticks.
// Used to report PID start times in clock ticks, just like /proc/<pid>/stat does.
static __inline uint64_t pl_nsec_to_clock_t(uint64_t x) {
  return div_u64(x, NSEC_PER_SEC / USER_HZ);
}

// Returns the group_leader offset.
// If GROUP_LEADER_OFFSET_OVERRIDE is defined, it is returned.
// Otherwise, the value is obtained from the definition of header structs.
// The override is important for the case when we don't have an exact header match.
// See user-space TaskStructResolver.
static __inline uint64_t task_struct_group_leader_offset() {
  if (GROUP_LEADER_OFFSET_OVERRIDE != 0) {
    return GROUP_LEADER_OFFSET_OVERRIDE;
  } else {
    return offsetof(struct task_struct, group_leader);
  }
}

// Returns the real_start_time/start_boottime offset.
// If START_BOOTTIME_OFFSET_OVERRIDE is defined, it is returned.
// Otherwise, the value is obtained from the definition of header structs.
// The override is important for the case when we don't have an exact header match.
// See user-space TaskStructResolver.
static __inline uint64_t task_struct_start_boottime_offset() {
  // Find the start_boottime of the current task.
  if (START_BOOTTIME_OFFSET_OVERRIDE != 0) {
    return START_BOOTTIME_OFFSET_OVERRIDE;
  } else {
    return offsetof(struct task_struct, START_BOOTTIME_VARNAME);
  }
}

// Effectively returns:
//   task->group_leader->start_boottime;  // Linux 5.5+
//   task->group_leader->real_start_time; // Linux 5.4 and earlier
static __inline uint64_t read_start_boottime(const struct task_struct* task) {
  uint64_t group_leader_offset = task_struct_group_leader_offset();
  struct task_struct* group_leader_ptr;
  bpf_probe_read(&group_leader_ptr, sizeof(struct task_struct*),
                 (uint8_t*)task + group_leader_offset);

  uint64_t start_boottime_offset = task_struct_start_boottime_offset();
  uint64_t start_boottime = 0;
  bpf_probe_read(&start_boottime, sizeof(uint64_t),
                 (uint8_t*)group_leader_ptr + start_boottime_offset);

  return pl_nsec_to_clock_t(start_boottime);
}

static __inline uint64_t get_tgid_start_time() {
  struct task_struct* task = (struct task_struct*)bpf_get_current_task();
  return read_start_boottime(task);
}
