/*
 * Copyright 2018- The Pixie Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
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
// We will conservatively assume that each sample inserts a new key into the stack_traces
// and into the histogram. Also, we will target some nominal time frame (the "transfer period")
// for the sampling to continue before doing the switch over.

// Given the targeted "transfer period" and the "sample period", we can find the
// number of entries required to be allocated in each of the maps,
// i.e. the number of expected stack traces:
// kExpectedStackTracesPerCPU = transfer_period / sample_period.
//
// Because sampling occurs per-cpu, the total number of expected stack traces is:
// kExpectedStackTraces = ncpus * kExpectedStackTracesPerCPU
//
// But, we will include some margin to make sure that hash collisions and
// data races do not cause us to drop data:
// kNumMapEntries = 4 * kExpectedStackTraces

// Notes:
// [1] A stack trace is an (ordered) vector of addresses (u64s), i.e.
// the set of instruction pointers found in the call stack at the moment
// the sample was triggered.

// Here, we compute the number of expected stack traces (used to allocate space
// in the shared BPF maps) per the notes above. NCPUS, TRANSFER_PERIOD, and SAMPLE_PERIOD
// pre-processor defines specified on the compiler command line (e.g. -DNCPUS=24).

#define DIV_ROUND_UP(NUM, DEN) ((NUM + DEN - 1) / DEN)

static const uint32_t kExpectedStackTracesPerCPU = DIV_ROUND_UP(TRANSFER_PERIOD, SAMPLE_PERIOD);
static const uint32_t kExpectedStackTraces = NCPUS * kExpectedStackTracesPerCPU;

// Oversize to avoid hash collisions.
static const uint32_t kNumMapEntries = 4 * kExpectedStackTraces;

// A threshold for checking that we've overrun the maps.
// This should be higher than kExpectedStackTraces due to timing variations,
// but it should be lower than kNumMapEntries.
static const uint32_t kSampleThreshold = 2 * kExpectedStackTraces;

BPF_PERF_OUTPUT(histogram_a);
BPF_PERF_OUTPUT(histogram_b);
BPF_STACK_TRACE(stack_traces_a, kNumMapEntries);
BPF_STACK_TRACE(stack_traces_b, kNumMapEntries);

// profiler_state: shared state vector between BPF & user space.
// See comments in shared header file "stack_event.h".
BPF_ARRAY(profiler_state, uint64_t, kProfilerStateVectorSize);

int sample_call_stack(struct bpf_perf_event_data* ctx) {
  int transfer_count_idx = kTransferCountIdx;
  int sample_count_a_idx = kSampleCountAIdx;
  int sample_count_b_idx = kSampleCountAIdx;
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

  // sample_count >= kSampleThreshold: indicates the number of samples taken has exceeded a
  // threshold such that we risk dropping data. User-space code should have read the data by now.
  // Report this error.
  if (sample_count >= kSampleThreshold) {
    uint64_t overflow_status_code = kOverflowError;
    profiler_state.update(&error_status_idx, &overflow_status_code);
    return 0;
  }

  return 0;
}
