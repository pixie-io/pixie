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

#include "src/carnot/planner/compiler/analyzer/combine_consecutive_maps_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;

using CombineConsecutiveMapsRuleTest = RulesTest;
TEST_F(CombineConsecutiveMapsRuleTest, basic) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression parent_expr{"count_1", MakeColumn("count", 0)};
  ColumnExpression child_expr{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map1 = MakeMap(mem_src, {parent_expr}, true);
  auto map2 = MakeMap(map1, {child_expr}, true);
  auto map2_id = map2->id();
  auto sink1 = MakeMemSink(map2, "abc");
  auto sink2 = MakeMemSink(map2, "def");

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(map1->id()));
  EXPECT_FALSE(graph->HasNode(map2_id));
  EXPECT_THAT(map1->Children(), ElementsAre(sink1, sink2));

  ASSERT_EQ(2, map1->col_exprs().size());
  EXPECT_EQ(parent_expr.name, map1->col_exprs()[0].name);
  EXPECT_EQ(parent_expr.node, map1->col_exprs()[0].node);
  EXPECT_EQ(child_expr.name, map1->col_exprs()[1].name);
  EXPECT_EQ(child_expr.node, map1->col_exprs()[1].node);
}

TEST_F(CombineConsecutiveMapsRuleTest, multiple_with_break) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};
  ColumnExpression expr3{"cpu_sum", MakeAddFunc(MakeColumn("cpu1", 0), MakeColumn("cpu2", 0))};
  // Should break here because cpu_sum was used prior
  ColumnExpression expr4{"cpu_sum_1", MakeColumn("cpu_sum", 0)};

  auto map1 = MakeMap(mem_src, {expr1}, true);
  auto map2 = MakeMap(map1, {expr2}, true);
  auto map3 = MakeMap(map2, {expr3}, true);
  auto map4 = MakeMap(map3, {expr4}, true);
  auto map2_id = map2->id();
  auto map3_id = map3->id();

  auto sink1 = MakeMemSink(map4, "abc");
  auto sink2 = MakeMemSink(map4, "def");

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(map1->id()));
  EXPECT_FALSE(graph->HasNode(map2_id));
  EXPECT_FALSE(graph->HasNode(map3_id));
  EXPECT_TRUE(graph->HasNode(map4->id()));
  EXPECT_THAT(map1->Children(), ElementsAre(map4));
  EXPECT_THAT(map4->Children(), ElementsAre(sink1, sink2));

  ASSERT_EQ(3, map1->col_exprs().size());
  EXPECT_EQ(expr1.name, map1->col_exprs()[0].name);
  EXPECT_EQ(expr1.node, map1->col_exprs()[0].node);
  EXPECT_EQ(expr2.name, map1->col_exprs()[1].name);
  EXPECT_EQ(expr2.node, map1->col_exprs()[1].node);
  EXPECT_EQ(expr3.name, map1->col_exprs()[2].name);
  EXPECT_EQ(expr3.node, map1->col_exprs()[2].node);
}

TEST_F(CombineConsecutiveMapsRuleTest, name_reassignment) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression parent_expr{"count_1", MakeColumn("count", 0)};
  ColumnExpression child_expr{"count_1", MakeColumn("count", 0)};

  auto map1 = MakeMap(mem_src, {parent_expr}, false);
  auto map2 = MakeMap(map1, {child_expr}, true);
  auto sink1 = MakeMemSink(map2, "abc");
  auto sink2 = MakeMemSink(map2, "def");
  auto map2_id = map2->id();

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(map1->id()));
  EXPECT_FALSE(graph->HasNode(map2_id));
  EXPECT_THAT(map1->Children(), ElementsAre(sink1, sink2));

  ASSERT_EQ(1, map1->col_exprs().size());
  EXPECT_EQ(child_expr.name, map1->col_exprs()[0].name);
  EXPECT_EQ(child_expr.node, map1->col_exprs()[0].node);
}

TEST_F(CombineConsecutiveMapsRuleTest, use_output_column) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression parent_expr{"count_1", MakeColumn("count", 0)};
  ColumnExpression child_expr{"sum", MakeAddFunc(MakeColumn("count", 0), MakeColumn("count_1", 0))};

  auto map1 = MakeMap(mem_src, {parent_expr}, false);
  auto map2 = MakeMap(map1, {child_expr}, true);
  MakeMemSink(map2, "abc");
  MakeMemSink(map2, "def");

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());
}

TEST_F(CombineConsecutiveMapsRuleTest, dependencies) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression parent_expr{"count_1", MakeColumn("count", 0)};
  ColumnExpression child_expr{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map1 = MakeMap(mem_src, {parent_expr}, true);
  MakeMap(map1, {child_expr}, true);
  MakeMemSink(map1, "abc");
  MakeMemSink(map1, "def");

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());
}

TEST_F(CombineConsecutiveMapsRuleTest, parent_dont_keep_input_columns) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression parent_expr{"count_1", MakeColumn("count", 0)};
  ColumnExpression child_expr{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map1 = MakeMap(mem_src, {parent_expr}, false);
  auto map2 = MakeMap(map1, {child_expr}, true);
  auto map2_id = map2->id();
  auto sink1 = MakeMemSink(map2, "abc");
  auto sink2 = MakeMemSink(map2, "def");

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(map1->id()));
  EXPECT_FALSE(graph->HasNode(map2_id));
  EXPECT_THAT(map1->Children(), ElementsAre(sink1, sink2));

  ASSERT_EQ(2, map1->col_exprs().size());
  EXPECT_EQ(parent_expr.name, map1->col_exprs()[0].name);
  EXPECT_EQ(parent_expr.node, map1->col_exprs()[0].node);
  EXPECT_EQ(child_expr.name, map1->col_exprs()[1].name);
  EXPECT_EQ(child_expr.node, map1->col_exprs()[1].node);
}

TEST_F(CombineConsecutiveMapsRuleTest, child_dont_keep_input_columns) {
  MemorySourceIR* mem_src = MakeMemSource();

  ColumnExpression parent_expr{"count_1", MakeColumn("count", 0)};
  ColumnExpression child_expr{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map1 = MakeMap(mem_src, {parent_expr}, true);
  MakeMap(map1, {child_expr}, false);
  MakeMemSink(map1, "abc");
  MakeMemSink(map1, "def");

  CombineConsecutiveMapsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
