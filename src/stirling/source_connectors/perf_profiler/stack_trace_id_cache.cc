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

#include <utility>

#include "src/stirling/source_connectors/perf_profiler/stack_trace_id_cache.h"

namespace px {
namespace stirling {
// TODO(jps): Add profiler namespace for all profiler code.

uint64_t StackTraceIDCache::Lookup(const profiler::SymbolicStackTrace& stack_trace) {
  // Case 1: Stack trace ID is in the current set. Just return it.
  const auto it = stack_trace_ids_.find(stack_trace);
  if (it != stack_trace_ids_.end()) {
    const uint64_t stack_trace_id = it->second;
    return stack_trace_id;
  }

  // Case 2: Stack trace ID is in the previous set. Copy it to current set, and return it.
  const auto it2 = prev_stack_trace_ids_.find(stack_trace);
  if (it2 != prev_stack_trace_ids_.end()) {
    const uint64_t stack_trace_id = it2->second;
    stack_trace_ids_[stack_trace] = stack_trace_id;
    return stack_trace_id;
  }

  // Case 3: Stack trace ID is not in the current nor the previous set. Create a new ID.
  const uint64_t stack_trace_id = ++next_stack_trace_id_;
  stack_trace_ids_[stack_trace] = stack_trace_id;
  return stack_trace_id;
}

void StackTraceIDCache::AgeTick() {
  prev_stack_trace_ids_ = std::move(stack_trace_ids_);
  stack_trace_ids_.clear();
}

}  // namespace stirling
}  // namespace px
