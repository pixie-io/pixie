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

#include <string>
#include <utility>

#include "src/shared/upid/upid.h"
#include "src/stirling/bpf_tools/bcc_symbolizer.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {
namespace profiler {
static constexpr upid_t kKernelUPID = {
    .pid = static_cast<uint32_t>(bpf_tools::BCCSymbolizer::kKernelPID), .start_time_ticks = 0};

/**
 * A function that takes an address as input and provides a symbolized string out.
 */
using SymbolizerFn = std::function<std::string_view(const uintptr_t addr)>;

// SymbolicStackTrace identifies a particular stack trace by:
// * upid
// * "folded" stack trace string
// The stack traces (in kernel & in BPF) are ordered lists of instruction pointers (addresses).
// Stirling uses BPF to recover the symbols associated with each address, and then
// uses the "symbolic stack trace" as the histogram key. Some of the stack traces that are
// distinct in the kernel and in BPF will collapse into the same symbolic stack trace in Stirling.
// For example, consider the following two stack traces from BPF:
// p0, p1, p2 => main;qux;baz   # both p2 & p3 point into baz.
// p0, p1, p3 => main;qux;baz
//
// SymbolicStackTrace will serve as a key to the unique stack-trace-id (an integer) in Stirling.
struct SymbolicStackTrace {
  const md::UPID upid;
  const std::string stack_trace_str;

  template <typename H>
  friend H AbslHashValue(H h, const SymbolicStackTrace& s) {
    return H::combine(std::move(h), s.upid, s.stack_trace_str);
  }

  friend bool operator==(const SymbolicStackTrace& lhs, const SymbolicStackTrace& rhs) {
    if (lhs.upid != rhs.upid) {
      return false;
    }
    return lhs.stack_trace_str == rhs.stack_trace_str;
  }
};

}  // namespace profiler
}  // namespace stirling
}  // namespace px
