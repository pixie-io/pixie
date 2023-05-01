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

#include <linux/bpf_perf_event.h>

#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf/utils.h"
#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

// This BPF probe samples stack-traces using two fundamental data structures:
// 1. stack_traces: a map from stack-trace [1] to stack-trace-id (an integer).
// 2. histogram: a map from stack-trace-id to observation count.
// The higher a count, the more we have observed a particular stack-trace,
// and the more likely something in that stack-trace is a potential perf. issue.

// To keep the stack-trace profiler "always on", we use a double buffering
// scheme wherein we allocate two of each data structure. Therefore,
// we have the following BPF tables:
// 1a. stack_traces_a,
// 1b. stack_traces_b,
// 2a. histogram_a, and
// 2b. histogram_b.

// Periodically, we need to switch over from map-set-a to map-set-b, and vice versa.
// We conservatively assume that each sample inserts a new key into the stack_traces
// and into the histogram. The transfer between sets is controlled by user-space.

// Notes:
// [1] A stack trace is an (ordered) vector of addresses (u64s), i.e.
// the set of instruction pointers found in the call stack at the moment
// the sample was triggered.

BPF_PERF_OUTPUT(histogram_a);
BPF_PERF_OUTPUT(histogram_b);
BPF_STACK_TRACE(stack_traces_a, CFG_STACK_TRACE_ENTRIES);
BPF_STACK_TRACE(stack_traces_b, CFG_STACK_TRACE_ENTRIES);

// profiler_state: shared state vector between BPF & user space.
// See comments in shared header file "stack_event.h".
BPF_ARRAY(profiler_state, uint64_t, kProfilerStateVectorSize);

int sample_call_stack(struct bpf_perf_event_data* ctx) {
  int transfer_count_idx = kTransferCountIdx;
  int sample_count_a_idx = kSampleCountAIdx;
  int sample_count_b_idx = kSampleCountBIdx;
  int error_status_idx = kErrorStatusIdx;

  uint64_t* transfer_count_ptr = profiler_state.lookup(&transfer_count_idx);
  uint64_t* sample_count_a_ptr = profiler_state.lookup(&sample_count_a_idx);
  uint64_t* sample_count_b_ptr = profiler_state.lookup(&sample_count_b_idx);

  if (transfer_count_ptr == NULL || sample_count_a_ptr == NULL || sample_count_b_ptr == NULL) {
    // One of the map lookups failed.
    // Set the appropriate error bit in the error bitfield:
    uint64_t rd_fail_status_code = kMapReadFailureError;
    profiler_state.update(&error_status_idx, &rd_fail_status_code);
    return 0;
  }

  uint64_t transfer_count = *transfer_count_ptr;

  // Create map key.
  struct stack_trace_key_t key = {};
  key.upid.tgid = bpf_get_current_pid_tgid() >> 32;
  key.upid.start_time_ticks = get_tgid_start_time();

  uint64_t sample_count = 0;

  if (transfer_count % 2 == 0) {
    // map set A branch:
    key.user_stack_id = stack_traces_a.get_stackid(&ctx->regs, BPF_F_USER_STACK);
    key.kernel_stack_id = stack_traces_a.get_stackid(&ctx->regs, 0);
    histogram_a.perf_submit(ctx, &key, sizeof(key));

    sample_count = *sample_count_a_ptr;
    *sample_count_a_ptr += 1;
  } else {
    // map set B branch:
    key.user_stack_id = stack_traces_b.get_stackid(&ctx->regs, BPF_F_USER_STACK);
    key.kernel_stack_id = stack_traces_b.get_stackid(&ctx->regs, 0);
    histogram_b.perf_submit(ctx, &key, sizeof(key));

    sample_count = *sample_count_b_ptr;
    *sample_count_b_ptr += 1;
  }

  // sample_count >= CFG_OVERRUN_THRESHOLD: indicates the number of samples taken has exceeded a
  // threshold such that we risk dropping data. User-space code should have read the data by now.
  // Report this error.
  if (sample_count >= CFG_OVERRUN_THRESHOLD) {
    uint64_t overflow_status_code = kOverflowError;
    profiler_state.update(&error_status_idx, &overflow_status_code);
    return 0;
  }

  return 0;
}
