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
#include <stdint.h>

#include <vector>

namespace px {
namespace stirling {

Stringifier::Stringifier(Symbolizer* symbolizer, ebpf::BPFStackTable* stack_traces)
    : symbolizer_(symbolizer), stack_traces_(stack_traces) {}

std::string Stringifier::BuildStackTraceString(const int stack_id, const struct upid_t& upid,
                                               const std::string_view& suffix) {
  // TODO(jps): re-evaluate the correct amount to reserve here.
  std::string stack_trace_str;
  stack_trace_str.reserve(128);

  // Get a function that returns a symbol based on an address. The upid is
  // used by the symbolizer both to find symbols in the underlying binary,
  // and to track cached symbols (symbols that we have previously looked up).
  auto symbolize_fn = symbolizer_->GetSymbolizerFn(upid);

  // Clear the stack-traces map as we go along here; this has lower overhead
  // compared to first reading the stack-traces map, then using clear_table_non_atomic().
  constexpr bool kClearStackId = true;

  // Some stack-traces have the address 0xcccccccccccccccc where one might
  // otherwise expect to find "main" or "start_thread". Given that this address
  // is not a "real" address, we filter it out below.
  constexpr uint64_t kSentinelAddr = 0xcccccccccccccccc;

  // Get the stack trace (as a vector of addresses) from the shared BPF stack trace table.
  const std::vector<uintptr_t> addrs = stack_traces_->get_stack_addr(stack_id, kClearStackId);
  VLOG_IF(1, addrs.size() == 0) << absl::Substitute(
      "[empty_stack_trace] stack_id: $0, upid.pid: $1", stack_id, static_cast<int>(upid.pid));

  // Build the folded stack trace string.
  for (auto iter = addrs.rbegin(); iter != addrs.rend(); ++iter) {
    const auto& addr = *iter;
    if (addr == kSentinelAddr && iter == addrs.rbegin()) {
      // Remove sentinel values at the root of the stack trace.
      // Sentinel values can occur in other spots (though it's rare); leave those ones in.
      continue;
    }
    stack_trace_str += symbolize_fn(addr);
    stack_trace_str += suffix;
    stack_trace_str += stringifier::kSeparator;
  }

  if (!stack_trace_str.empty()) {
    // Remove trailing separator.
    stack_trace_str.pop_back();
  }

  return stack_trace_str;
}

std::string Stringifier::FindOrBuildStackTraceString(const int stack_id, const struct upid_t& upid,
                                                     const std::string_view& suffix) {
  // First try to find the memoized result in the stack_trace_strs_ map,
  // if no memoized result is available, build the folded stack trace string.
  auto [iter, inserted] = stack_trace_strs_.try_emplace(stack_id, "");
  if (inserted) {
    iter->second = BuildStackTraceString(stack_id, upid, suffix);
  }
  return iter->second;
}

std::string Stringifier::FoldedStackTraceString(const stack_trace_key_t& key) {
  const int u_stack_id = key.user_stack_id;
  const int k_stack_id = key.kernel_stack_id;

  const struct upid_t& u_upid = key.upid;
  const struct upid_t& k_upid = profiler::kKernelUPID;

  // Using bind because it helps the reduce redunant information in the if/else chain below.
  // Also, it is easier to read, e.g.:
  // stack_trace_str = u_stack_str_fn() + ";" + k_stack_str_fn();
  auto fn_addr = &Stringifier::FindOrBuildStackTraceString;
  auto u_stack_str_fn = std::bind(fn_addr, this, u_stack_id, u_upid, stringifier::kUserSuffix);
  auto k_stack_str_fn = std::bind(fn_addr, this, k_stack_id, k_upid, stringifier::kKernSuffix);

  std::string stack_trace_str;
  stack_trace_str.reserve(128);

  // TODO(jps/oazizi): question... should we use the "drop message" for -EEXIST,
  // if only one of two stack-ids indicates a hash table collision?
  // vs. the current logic which shows the "drop message" only if both stack-ids are -EEXIST.

  if (u_stack_id >= 0 && k_stack_id >= 0) {
    stack_trace_str = u_stack_str_fn();
    stack_trace_str += stringifier::kSeparator;
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
    stack_trace_str = stringifier::kDropMessage;
    DCHECK(u_stack_id == -EEXIST || u_stack_id == -EFAULT) << "u_stack_id: " << u_stack_id;
    DCHECK(k_stack_id == -EEXIST || k_stack_id == -EFAULT) << "k_stack_id: " << k_stack_id;
    DCHECK(!(k_stack_id == -EFAULT && u_stack_id == -EFAULT)) << "both invalid.";
  }

  return stack_trace_str;
}

}  // namespace stirling
}  // namespace px
