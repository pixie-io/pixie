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

#include <absl/container/flat_hash_map.h>

#include "src/stirling/source_connectors/perf_profiler/shared/types.h"

namespace px {
namespace stirling {

// The StackTraceIDCache maintains a mapping of stack trace strings to stack trace IDs.
// We maintain these IDs for a number of reasons:
//  1) The IDs enable more efficient aggregations across time samples in Carnot:
//     aggregations with integers are more efficient than aggregations with strings.
//  2) The IDs enable table normalization (future work).
//
// As a cache, it should be noted that no guarantee is made that a stack trace from one time
// period is assigned the same stack trace ID. Any consumer of the data can only assume that
// two records with identical stack trace IDs will have identical stack trace strings. The
// reverse, however, is not true: Two records with identical stack trace strings may not have
// the same stack trace ID.
//
// We allow identical stack trace strings to map to different stack trace id integers to bound
// the total memory consumed by this map. Otherwise the map's memory usage grows unbounded.
//
// For cases where the same stack trace ID shows up with different IDs,
// the UI will aggregate the identical stack traces for us in the visualization.
class StackTraceIDCache {
 public:
  uint64_t Lookup(const profiler::SymbolicStackTrace& stack_trace);
  void AgeTick();

 private:
  absl::flat_hash_map<profiler::SymbolicStackTrace, uint64_t> stack_trace_ids_;
  absl::flat_hash_map<profiler::SymbolicStackTrace, uint64_t> prev_stack_trace_ids_;

  // Tracks the next stack-trace-id to be assigned;
  // incremented by 1 for each such assignment.
  uint64_t next_stack_trace_id_ = 0;
};

}  // namespace stirling
}  // namespace px
