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
