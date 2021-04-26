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

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

namespace px {
namespace stirling {

// Test the symbolizer with caching enabled and disabled.
TEST(SymbolizerTest, Basic) {
  bpf_tools::BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_STACK_TRACE(test_stack_traces, 1024);";
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kProgram));
  ebpf::BPFStackTable stack_table = bcc_wrapper.GetStackTable("test_stack_traces");

  FLAGS_stirling_profiler_symcache = true;
  const bool enable_symbolization = true;
  Symbolizer symbolizer(Symbolizer::kKernelPID, enable_symbolization);

  // Lookup an address for the first time. This should be a cache miss.
  EXPECT_EQ(symbolizer.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(symbolizer.stat_accesses(), 1);
  EXPECT_EQ(symbolizer.stat_hits(), 0);

  // Lookup the address a second time. We should get a cache hit.
  EXPECT_EQ(symbolizer.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(symbolizer.stat_accesses(), 2);
  EXPECT_EQ(symbolizer.stat_hits(), 1);

  symbolizer.FlushCache();

  // Lookup the address again, but now we should *not* get a cache hit.
  EXPECT_EQ(symbolizer.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(symbolizer.stat_accesses(), 3);
  EXPECT_EQ(symbolizer.stat_hits(), 1);

  // Expect a cache hit:
  EXPECT_EQ(symbolizer.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(symbolizer.stat_accesses(), 4);
  EXPECT_EQ(symbolizer.stat_hits(), 2);

  // Turn off caching, expect stats to not change:
  FLAGS_stirling_profiler_symcache = false;
  EXPECT_EQ(symbolizer.LookupSym(&stack_table, 0), "0x0000000000000000");
  EXPECT_EQ(symbolizer.stat_accesses(), 4);
  EXPECT_EQ(symbolizer.stat_hits(), 2);
}

// A test w/ symbolization disabled. We expect the symbolizer to simply
// return a stringified version of the input address.
TEST(SymbolizerTest, TestSymbolizationPolicy) {
  bpf_tools::BCCWrapper bcc_wrapper;
  std::string_view kProgram = "BPF_STACK_TRACE(test_stack_traces, 1024);";
  ASSERT_OK(bcc_wrapper.InitBPFProgram(kProgram));
  ebpf::BPFStackTable stack_table = bcc_wrapper.GetStackTable("test_stack_traces");

  // Create a dummy symbolizer that simply echoes back the address as a string.
  const bool disableSymbolization = false;
  Symbolizer dummy(Symbolizer::kKernelPID, disableSymbolization);
  EXPECT_EQ(dummy.LookupSym(&stack_table, 1), "0x0000000000000001");
  EXPECT_EQ(dummy.LookupSym(&stack_table, 2), "0x0000000000000002");
  EXPECT_EQ(dummy.stat_accesses(), 0);
  EXPECT_EQ(dummy.stat_hits(), 0);
}

}  // namespace stirling
}  // namespace px
