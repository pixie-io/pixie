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

#include "src/carnot/planner/compiler/analyzer/resolve_stream_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;

TEST_F(RulesTest, resolve_stream) {
  MemorySourceIR* src = MakeMemSource();
  FilterIR* filter = MakeFilter(src);
  StreamIR* stream = graph->CreateNode<StreamIR>(ast, filter).ValueOrDie();
  MemorySinkIR* sink = MakeMemSink(stream, "");
  auto stream_id = stream->id();

  EXPECT_TRUE(graph->dag().HasNode(stream_id));
  EXPECT_FALSE(src->streaming());

  ResolveStreamRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_FALSE(graph->dag().HasNode(stream_id));
  EXPECT_TRUE(src->streaming());
  EXPECT_THAT(sink->parents(), ElementsAre(filter));
}

TEST_F(RulesTest, resolve_stream_no_stream) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg, "");

  ResolveStreamRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(RulesTest, resolve_stream_blocking_ancestor) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  StreamIR* stream = graph->CreateNode<StreamIR>(ast, agg).ValueOrDie();
  MakeMemSink(stream, "");

  ResolveStreamRule rule;
  ASSERT_NOT_OK(rule.Execute(graph.get()));
}

TEST_F(RulesTest, resolve_stream_non_mem_sink_child) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  StreamIR* stream = graph->CreateNode<StreamIR>(ast, agg).ValueOrDie();
  MakeMemSink(stream, "1");
  FilterIR* filter = MakeFilter(stream);
  MakeMemSink(filter, "2");

  ResolveStreamRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_NOT_OK(result);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
