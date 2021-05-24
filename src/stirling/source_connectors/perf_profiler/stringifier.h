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

#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

namespace px {
namespace stirling {

namespace stringifier {
// The separator between symbols in the folded stack trace string.
// e.g. main;foo;bar;printf
constexpr std::string_view kSeparator = ";";

// The suffix is attached to each symbol in a folded stack trace string.
static constexpr std::string_view kUserSuffix = "";
static constexpr std::string_view kKernSuffix = "_[k]";

// The drop message indicates that the kernel had a hash table collision
// and dropped tracking of one stack trace.
static constexpr std::string_view kDropMessage = "<stack trace lost>";
}  // namespace stringifier

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
// a destructive read (it read one stack trace, and clears it, from the table).
// Because of stack-trace-id reuse and the destructive read, the stringifier memoizes
// its stringified results. A new stringifier is created (and destroyed) on each iteration
// of the continuous perf. profiler.
class Stringifier {
 public:
  Stringifier(Symbolizer* symbolizer, ebpf::BPFStackTable* stack_traces);

  // Returns a folded stack trace string based on the stack trace histogram key.
  // The key contains both a user & kernel stack-trace-id, which are subsquently
  // passed into FindOrBuildStackTraceString().
  std::string FoldedStackTraceString(const stack_trace_key_t& key);

 private:
  std::string BuildStackTraceString(const int stack_id, const struct upid_t& upid,
                                    const std::string_view& suffix);
  std::string FindOrBuildStackTraceString(const int stack_id, const struct upid_t& upid,
                                          const std::string_view& suffix);

  // Memoized results of previous calls to FindOrBuildStackTraceString():
  // a map from stack-trace-id to folded stack trace string.
  absl::flat_hash_map<int, std::string> stack_trace_strs_;

  // The symbolizer is used to look up a symbol that corresponds to a stack trace address.
  Symbolizer* const symbolizer_;

  // The shared BPF stack trace table provides a stack trace, as a list of addresses,
  // based on the stack-trace-id (an integer). When read, a given stack trace is consumed by
  // a destructive read, i.e. such that the BPF stack trace table does not need
  // to be explicitly cleared (by re-iterating the histogram) after an iteration
  // of the continuous perf. profiler is completed.
  ebpf::BPFStackTable* const stack_traces_;
};

}  // namespace stirling
}  // namespace px
