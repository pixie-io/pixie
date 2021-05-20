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

#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

static const uint32_t kNumMapEntries = 4096;

BPF_HASH(histogram, struct stack_trace_key_t, uint64_t, kNumMapEntries);
BPF_STACK_TRACE(stack_traces, kNumMapEntries);

// A minimal BPF program to grab a stack trace, and push it into
// a shared BPF stack traces map. For convenience, we will also build a histogram
// to record the stack trace keys, which have the stack-ids used by the stack traces map.
int stack_trace_sampler(struct pt_regs* ctx) {
  // Create map key.
  struct stack_trace_key_t key = {};
  key.upid.tgid = bpf_get_current_pid_tgid() >> 32;
  key.upid.start_time_ticks = 0;

  key.user_stack_id = stack_traces.get_stackid(ctx, BPF_F_USER_STACK);
  key.kernel_stack_id = stack_traces.get_stackid(ctx, 0);
  histogram.increment(key);
  return 0;
}
