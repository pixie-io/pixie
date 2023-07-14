/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
#include <utility>
#endif

#include "src/stirling/upid/upid.h"

// TODO(jps): add a macro that wraps bpf_trace_printk for debug & no-ops for prod builds.

// Indices into the profiler shared state vector "profiler_state":
// profiler_state[0]: transfer count          # written on user side, read on BPF side
// profiler_state[1]: sample count A          # updated on BPF side, reset on user side
// profiler_state[2]: sample count B          # updated on BPF side, reset on user side
// profiler_state[3]: error status bitfield   # written on BPF side, read on user side
// TODO(jps): Consider switching to a C-style enum.
static const uint32_t kTransferCountIdx = 0;
static const uint32_t kSampleCountAIdx = 1;
static const uint32_t kSampleCountBIdx = 2;
static const uint32_t kErrorStatusIdx = 3;
static const uint32_t kProfilerStateVectorSize = 4;

// stack_trace_key_t indexes into the stack-trace histogram.
// By tying together the user & kernel stack-trace-ids [1],
// it fully identifies a unique stack trace.
//
// [1] user & kernel stack trace ids are tracked separately (the kernel creates
// user & kernel stacks separately because of address aliasing).
struct stack_trace_key_t {
  struct upid_t upid;

  // user_stack_id, an index into the stack-traces map.
  int user_stack_id;

  // kernel_stack_id, an index into the stack-traces map.
  int kernel_stack_id;

#ifdef __cplusplus
  template <typename H>
  friend H AbslHashValue(H h, const stack_trace_key_t& s) {
    return H::combine(std::move(h), s.upid, s.user_stack_id, s.kernel_stack_id);
  }

  friend bool operator==(const stack_trace_key_t& lhs, const stack_trace_key_t& rhs) {
    if (lhs.upid != rhs.upid) {
      return false;
    }
    if (lhs.user_stack_id != rhs.user_stack_id) {
      return false;
    }
    return lhs.kernel_stack_id == rhs.kernel_stack_id;
  }
#endif
};

// Bit positions in the error status bitfield:
static const uint32_t kOverflowBitPos = 0;
static const uint32_t kMapReadFailureBitPos = 1;

// The error codes, themselves:
static const uint64_t kPerfProfilerStatusOk = 0ULL;
static const uint64_t kOverflowError = 1ULL << kOverflowBitPos;
static const uint64_t kMapReadFailureError = 1ULL << kMapReadFailureBitPos;

#ifdef __cplusplus
static constexpr std::string_view kHistogramAName = "histogram_a";
static constexpr std::string_view kHistogramBName = "histogram_b";
#endif
