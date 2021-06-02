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

#include <string>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/optimizer/prune_unconnected_operators_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;

using PruneUnconnectedOperatorsRuleTest = RulesTest;

TEST_F(PruneUnconnectedOperatorsRuleTest, basic) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map1 = MakeMap(mem_src, {expr1}, false);
  Relation map1_relation{{types::DataType::INT64}, {"count_1"}};
  ASSERT_OK(map1->SetRelation(map1_relation));
  auto map1_id = map1->id();

  auto map2 = MakeMap(mem_src, {expr2}, false);
  Relation map2_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};
  ASSERT_OK(map2->SetRelation(map2_relation));
  auto map2_id = map2->id();

  auto sink = MakeMemSink(map2, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};
  ASSERT_OK(sink->SetRelation(sink_relation));

  PruneUnconnectedOperatorsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(map2_id));
  EXPECT_FALSE(graph->HasNode(map1_id));

  // Should be unchanged
  EXPECT_EQ(sink_relation, sink->relation());
}

TEST_F(PruneUnconnectedOperatorsRuleTest, unchanged) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());

  auto count_col = MakeColumn("count", 0);
  auto cpu1_col = MakeColumn("cpu1", 0);
  auto cpu2_col = MakeColumn("cpu2", 0);
  auto cpu_sum = MakeAddFunc(cpu1_col, cpu2_col);
  ColumnExpression expr1{"count_1", count_col};
  ColumnExpression expr2{"cpu_sum", cpu_sum};
  ColumnExpression expr3{"cpu1_1", cpu1_col};

  auto map1 = MakeMap(mem_src, {expr1, expr2}, false);
  auto map2 = MakeMap(mem_src, {expr1, expr3}, false);

  MakeMemSink(map1, "out1", {"count_1", "cpu_sum"});
  MakeMemSink(map2, "out2", {"count_1", "cpu1_1"});

  auto nodes_before = graph->dag().TopologicalSort();

  PruneUnconnectedOperatorsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());

  EXPECT_EQ(nodes_before, graph->dag().TopologicalSort());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
