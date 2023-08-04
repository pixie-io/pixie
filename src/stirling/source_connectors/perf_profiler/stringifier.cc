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

#include "src/stirling/source_connectors/perf_profiler/stringifier.h"
#include "src/stirling/source_connectors/perf_profiler/shared/symbolization.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include <absl/functional/bind_front.h>

namespace px {
namespace stirling {

Stringifier::Stringifier(Symbolizer* u_symbolizer, Symbolizer* k_symbolizer,
                         WrappedBCCStackTable* stack_traces)
    : u_symbolizer_(u_symbolizer), k_symbolizer_(k_symbolizer), stack_traces_(stack_traces) {}

std::string Stringifier::BuildStackTraceString(const std::vector<uintptr_t>& addrs,
                                               profiler::SymbolizerFn symbolize_fn,
                                               const std::string_view& prefix) {
  using symbolization::kJavaInterpreter;
  using symbolization::kSeparator;

  // TODO(jps): re-evaluate the correct amount to reserve here.
  std::string stack_trace_str;
  stack_trace_str.reserve(128);

  // Some stack-traces have the address 0xcccccccccccccccc where one might
  // otherwise expect to find "main" or "start_thread". Given that this address
  // is not a "real" address, we filter it out below.
  constexpr uint64_t kSentinelAddr = 0xcccccccccccccccc;
  uint64_t num_collapsed = 0;

  // Build the folded stack trace string.
  for (auto iter = addrs.rbegin(); iter != addrs.rend(); ++iter) {
    const auto& addr = *iter;
    if (addr == kSentinelAddr && iter == addrs.rbegin()) {
      // Remove sentinel values at the root of the stack trace.
      // Sentinel values can occur in other spots (though it's rare); leave those ones in.
      continue;
    }
    const std::string_view symbol = symbolize_fn(addr);
    if (symbol == kJavaInterpreter) {
      ++num_collapsed;
      continue;
    } else if (num_collapsed > 0) {
      stack_trace_str =
          absl::StrCat(stack_trace_str, kJavaInterpreter, " [", num_collapsed, "x]", kSeparator);
      num_collapsed = 0;
    }
    stack_trace_str = absl::StrCat(stack_trace_str, prefix, symbol, kSeparator);
  }
  if (num_collapsed) {
    stack_trace_str =
        absl::StrCat(stack_trace_str, kJavaInterpreter, " [", num_collapsed, "x]", kSeparator);
  }

  if (!stack_trace_str.empty()) {
    // Remove trailing separator.
    stack_trace_str.pop_back();
  }

  return stack_trace_str;
}

std::string Stringifier::FindOrBuildStackTraceString(const int stack_id,
                                                     profiler::SymbolizerFn symbolize_fn,
                                                     const std::string_view& prefix) {
  // First try to find the memoized result in the stack_trace_strs_ map,
  // if no memoized result is available, build the folded stack trace string.
  auto [iter, inserted] = stack_trace_strs_.try_emplace(stack_id, "");
  if (inserted) {
    // Clear the stack-traces map as we go along here; this has lower overhead
    // compared to first reading the stack-traces map, then using clear_table_non_atomic().
    constexpr bool kClearStackId = true;

    // Get the stack trace (as a vector of addresses) from the shared BPF stack trace table.
    const std::vector<uintptr_t> addrs = stack_traces_->GetStackAddr(stack_id, kClearStackId);
    VLOG_IF(1, addrs.empty()) << absl::Substitute("[empty_stack_trace] stack_id: $0", stack_id);

    iter->second = BuildStackTraceString(addrs, symbolize_fn, prefix);
  }
  return iter->second;
}

std::string Stringifier::FoldedStackTraceString(const stack_trace_key_t& key) {
  using symbolization::kKernelPrefix;
  using symbolization::kUserPrefix;

  const int u_stack_id = key.user_stack_id;
  const int k_stack_id = key.kernel_stack_id;

  const struct upid_t& u_upid = key.upid;
  const struct upid_t& k_upid = profiler::kKernelUPID;

  auto u_symbolizer_fn = u_symbolizer_->GetSymbolizerFn(u_upid);
  auto k_symbolizer_fn = k_symbolizer_->GetSymbolizerFn(k_upid);

  // Using bind because it helps reduce redundant information in the if/else chain below.
  // Also, it is easier to read, e.g.:
  // stack_trace_str = u_stack_str_fn() + ";" + k_stack_str_fn();
  auto fn_addr = &Stringifier::FindOrBuildStackTraceString;
  auto u_stack_str_fn = absl::bind_front(fn_addr, this, u_stack_id, u_symbolizer_fn, kUserPrefix);
  auto k_stack_str_fn = absl::bind_front(fn_addr, this, k_stack_id, k_symbolizer_fn, kKernelPrefix);

  std::string stack_trace_str;
  stack_trace_str.reserve(128);

  // TODO(jps/oazizi): question... should we use the "drop message" for -EEXIST,
  // if only one of two stack-ids indicates a hash table collision?
  // vs. the current logic which shows the "drop message" only if both stack-ids are -EEXIST.

  if (u_stack_id >= 0 && k_stack_id >= 0) {
    stack_trace_str = u_stack_str_fn();
    stack_trace_str += symbolization::kSeparator;
    stack_trace_str += k_stack_str_fn();
  } else if (u_stack_id >= 0) {
    stack_trace_str = u_stack_str_fn();
    DCHECK(k_stack_id == -EEXIST || k_stack_id == -EFAULT) << "ustack_id: " << u_stack_id;
  } else if (k_stack_id >= 0) {
    stack_trace_str = k_stack_str_fn();
    DCHECK(u_stack_id == -EEXIST || u_stack_id == -EFAULT) << "kstack_id: " << k_stack_id;
  } else {
    // The kernel can indicate "not valid" for a stack-id in two different ways:
    // 1. -EFAULT: the stack trace was not available
    // e.g. stack trace is in user space only and kstack_id is invalid.
    // 2. -EEXIST: hash bucket collision in the stack traces table
    // We can reach this branch if one, or both, of the stack-ids had a hash table collision,
    // but we should not get here with both stack-ids set to "invalid" i.e. -EFAULT.
    stack_trace_str = symbolization::kDropMessage;
    DCHECK(u_stack_id == -EEXIST || u_stack_id == -EFAULT) << "u_stack_id: " << u_stack_id;
    DCHECK(k_stack_id == -EEXIST || k_stack_id == -EFAULT) << "k_stack_id: " << k_stack_id;
    DCHECK(!(k_stack_id == -EFAULT && u_stack_id == -EFAULT)) << "both invalid.";
  }

  return stack_trace_str;
}

}  // namespace stirling
}  // namespace px
