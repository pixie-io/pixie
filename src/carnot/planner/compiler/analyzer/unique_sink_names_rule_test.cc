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

#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/unique_sink_names_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

TEST_F(RulesTest, UniqueSinkNameRule) {
  MemorySourceIR* mem_src = MakeMemSource();
  MemorySinkIR* foo1 = MakeMemSink(mem_src, "foo");
  MemorySinkIR* foo2 = MakeMemSink(mem_src, "foo");
  GRPCSinkIR* foo3 = MakeGRPCSink(mem_src, "foo", std::vector<std::string>{});
  MemorySinkIR* bar1 = MakeMemSink(mem_src, "bar");
  MemorySinkIR* bar2 = MakeMemSink(mem_src, "bar");
  MemorySinkIR* abc = MakeMemSink(mem_src, "abc");

  UniqueSinkNameRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  std::vector<std::string> expected_sink_names{"foo", "foo_1", "foo_2", "bar", "bar_1", "abc"};
  std::vector<OperatorIR*> sinks{foo1, foo2, foo3, bar1, bar2, abc};
  for (const auto& [idx, op] : Enumerate(sinks)) {
    std::string sink_name;
    if (Match(op, MemorySink())) {
      sink_name = static_cast<MemorySinkIR*>(op)->name();
    } else {
      sink_name = static_cast<GRPCSinkIR*>(op)->name();
    }
    EXPECT_EQ(sink_name, expected_sink_names[idx]);
  }
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
