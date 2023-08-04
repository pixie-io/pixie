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
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/symbolizer.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

using bpf_tools::WrappedBCCStackTable;

// Stringifier serves two purposes:
// 1. constructs a "folded stack trace string" based on the stack frame addresses.
// 2. memoizes previous results of (1) above in case a "stack-id" is reused
//
// A folded stack trace string looks like this (taken from the perf profiler test):
// __libc_start_main;main;fib52();fib(unsigned long)
// It is a list of symbols that correspond to the addresses in the underlying stack trace,
// with each symbol separated by ';'.
//
// Because the kernel collects stack-traces separately for user & kernel space,
// at the BPF level, we track stack traces with a key that includes two "stack-trace-ids",
// one for user space and one for kernel. For this reason, we commonly see reuse of individual
// stack trace ids...
// e.g. in the case where the same kernel stack trace is observed from multiple user stack traces,
// or in the case where a given user space stack sometimes (but not always) enters the kernel.
//
// When the stringifier reads the shared BPF map of stack trace addresses, it does so using
// a destructive read (it reads one stack trace, and clears it, from the table).
// Because of stack-trace-id reuse and the destructive read, the stringifier memoizes
// its stringified results. A new stringifier is created (and destroyed) on each iteration
// of the continuous perf. profiler.
class Stringifier {
 public:
  /**
   * Construct a stack trace stringifier.
   *
   * @param u_symbolizer A symbolizer for user-space addresses.
   * @param k_symbolizer A symbolizer for kernel-space addresses.
   * @param stack_traces Pointer to the BCC collected stack traces.
   */
  Stringifier(Symbolizer* u_symbolizer, Symbolizer* k_symbolizer,
              WrappedBCCStackTable* stack_traces);

  // Returns a folded stack trace string based on the stack trace histogram key.
  // The key contains both a user & kernel stack-trace-id, which are subsequently
  // passed into FindOrBuildStackTraceString().
  std::string FoldedStackTraceString(const stack_trace_key_t& key);

 private:
  std::string BuildStackTraceString(const std::vector<uintptr_t>& addrs,
                                    profiler::SymbolizerFn symbolize_fn,
                                    const std::string_view& prefix);
  std::string FindOrBuildStackTraceString(const int stack_id, profiler::SymbolizerFn symbolize_fn,
                                          const std::string_view& prefix);

  // Memoized results of previous calls to FindOrBuildStackTraceString():
  // a map from stack-trace-id to folded stack trace string.
  absl::flat_hash_map<int, std::string> stack_trace_strs_;

  // The symbolizer is used to look up a symbol that corresponds to a stack trace address.
  Symbolizer* const u_symbolizer_;
  Symbolizer* const k_symbolizer_;

  // The shared BPF stack trace table provides a stack trace, as a list of addresses,
  // based on the stack-trace-id (an integer). When read, a given stack trace is consumed by
  // a destructive read, i.e. such that the BPF stack trace table does not need
  // to be explicitly cleared (by re-iterating the histogram) after an iteration
  // of the continuous perf. profiler is completed.
  WrappedBCCStackTable* const stack_traces_;
};

}  // namespace stirling
}  // namespace px
