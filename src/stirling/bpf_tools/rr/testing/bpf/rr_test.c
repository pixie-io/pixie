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
#include <linux/ptrace.h>

BPF_PERF_OUTPUT(perf_buffer);
BPF_HASH(map, int, int);
BPF_ARRAY(state, int, 1);
BPF_ARRAY(results, int, 1024);
BPF_STACK_TRACE(stack_table, 1024);
BPF_PERF_OUTPUT(stack_ids);

// A minimal BPF program to:
// 1. Push something into a perf buffer.
// 2. Populate a BPF array.
// 3. Populate a BPF hash map.
int push_something_into_a_perf_buffer(struct pt_regs* ctx) {
  int count_idx = 0;
  int* pcount = state.lookup(&count_idx);
  if (pcount == NULL) {
    return -1;
  }
  int count = *pcount;

  int arg_val = PT_REGS_PARM1(ctx);
  perf_buffer.perf_submit(ctx, &arg_val, sizeof(int));
  map.update(&count, &arg_val);
  results.update(&count, &arg_val);

  ++count;
  state.update(&count_idx, &count);
  return 0;
}

// A minimal BPF program to sample a stack trace.
int sample_a_stack_trace(struct pt_regs* ctx) {
  int stack_id = stack_table.get_stackid(ctx, BPF_F_USER_STACK);
  stack_ids.perf_submit(ctx, &stack_id, sizeof(int));
  return 0;
}
