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

#include <gtest/gtest.h>

#include "src/stirling/source_connectors/perf_profiler/stack_trace_id_cache.h"

namespace px {
namespace stirling {

TEST(StackTraceIDCache, Basic) {
  StackTraceIDCache stack_trace_ids;

  const md::UPID kUPID(1, 1, 1);
  const profiler::SymbolicStackTrace kStackTrace1{kUPID, "a();b();c();"};
  const profiler::SymbolicStackTrace kStackTrace2{kUPID, "d();e();f();"};

  uint64_t id1 = stack_trace_ids.Lookup(kStackTrace1);
  uint64_t id2 = stack_trace_ids.Lookup(kStackTrace2);

  // Check for consistency.
  EXPECT_EQ(stack_trace_ids.Lookup(kStackTrace1), id1);
  EXPECT_EQ(stack_trace_ids.Lookup(kStackTrace2), id2);

  stack_trace_ids.AgeTick();

  // Maintain IDs across one generation.
  EXPECT_EQ(stack_trace_ids.Lookup(kStackTrace1), id1);
  EXPECT_EQ(stack_trace_ids.Lookup(kStackTrace2), id2);

  stack_trace_ids.AgeTick();
  stack_trace_ids.AgeTick();

  // Expect new IDs if too many generations have passed since last use.
  EXPECT_NE(stack_trace_ids.Lookup(kStackTrace1), id1);
  EXPECT_NE(stack_trace_ids.Lookup(kStackTrace2), id2);
}

}  // namespace stirling
}  // namespace px
