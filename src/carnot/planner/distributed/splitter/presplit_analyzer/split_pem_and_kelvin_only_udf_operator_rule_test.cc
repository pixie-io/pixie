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

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/splitter/presplit_analyzer/split_pem_and_kelvin_only_udf_operator_rule.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using compiler::ResolveTypesRule;
using ::testing::ElementsAre;

using SplitPEMAndKelvinOnlyUDFOperatorRuleTest = testutils::DistributedRulesTest;
TEST_F(SplitPEMAndKelvinOnlyUDFOperatorRuleTest, noop) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto func1 = MakeEqualsFunc(MakeInt(3), MakeInt(2));
  MapIR* map1 = MakeMap(src1, {{"out", func1}});
  MakeMemSink(map1, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  SplitPEMAndKelvinOnlyUDFOperatorRule rule(compiler_state_.get());

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());
}

TEST_F(SplitPEMAndKelvinOnlyUDFOperatorRuleTest, simple) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  ASSERT_OK(ResolveOperatorType(src1, compiler_state_.get()));
  auto input1 = MakeColumn("remote_addr", 0);
  auto input2 = MakeColumn("req_path", 0);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {input2});
  MapIR* map1 = MakeMap(src1, {{"pem", func1}, {"kelvin", func2}});
  MemorySinkIR* sink = MakeMemSink(map1, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  Relation existing_map_relation({types::STRING, types::STRING}, {"pem", "kelvin"});
  EXPECT_THAT(*map1->resolved_table_type(), IsTableType(existing_map_relation));

  SplitPEMAndKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], map1);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING, types::STRING}, {"pem_only_0", "req_path"});
  EXPECT_THAT(*new_map->resolved_table_type(), IsTableType(expected_map_relation));
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(map1));

  // original map relation and children shouldn't have changed.
  // pem_only func should now be a column projection.
  EXPECT_THAT(*map1->resolved_table_type(), IsTableType(existing_map_relation));
  EXPECT_EQ(2, map1->col_exprs().size());
  EXPECT_EQ("pem", map1->col_exprs()[0].name);
  EXPECT_MATCH(map1->col_exprs()[0].node, ColumnNode("pem_only_0"));
  EXPECT_EQ("kelvin", map1->col_exprs()[1].name);
  EXPECT_MATCH(map1->col_exprs()[1].node,
               FuncNameAllArgsMatch("kelvin_only", ColumnNode("req_path")));
  EXPECT_THAT(map1->parents(), ElementsAre(new_map));
  EXPECT_THAT(map1->Children(), ElementsAre(sink));
}

TEST_F(SplitPEMAndKelvinOnlyUDFOperatorRuleTest, nested) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto input1 = MakeColumn("remote_addr", 0);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {func1});
  MapIR* map1 = MakeMap(src1, {{"kelvin", func2}});
  MemorySinkIR* sink = MakeMemSink(map1, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  Relation existing_map_relation({types::STRING}, {"kelvin"});
  EXPECT_THAT(*map1->resolved_table_type(), IsTableType(existing_map_relation));

  SplitPEMAndKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], map1);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING}, {"pem_only_0"});
  EXPECT_THAT(*new_map->resolved_table_type(), IsTableType(expected_map_relation));
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(map1));

  // original map relation and children shouldn't have changed.
  EXPECT_THAT(*map1->resolved_table_type(), IsTableType(existing_map_relation));
  EXPECT_EQ(1, map1->col_exprs().size());
  EXPECT_EQ("kelvin", map1->col_exprs()[0].name);
  EXPECT_MATCH(map1->col_exprs()[0].node,
               FuncNameAllArgsMatch("kelvin_only", ColumnNode("pem_only_0")));
  EXPECT_THAT(map1->parents(), ElementsAre(new_map));
  EXPECT_THAT(map1->Children(), ElementsAre(sink));
}

TEST_F(SplitPEMAndKelvinOnlyUDFOperatorRuleTest, name_collision) {
  MemorySourceIR* src1 = MakeMemSource("http_events");

  auto input1 = MakeColumn("remote_addr", 0);
  MapIR* map1 = MakeMap(src1, {{"pem_only_0", input1}});

  auto input2 = MakeColumn("pem_only_0", 0);
  auto func2 = MakeFunc("pem_only", {input2});
  auto func3 = MakeFunc("kelvin_only", {input2});
  MapIR* map2 = MakeMap(map1, {{"pem_only_0", func2}, {"kelvin_only", func3}});
  MakeMemSink(map2, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  SplitPEMAndKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  EXPECT_EQ(1, map1->Children().size());
  EXPECT_MATCH(map1->Children()[0], Map());
  auto inserted_map = static_cast<MapIR*>(map1->Children()[0]);
  auto expected_relation = Relation({types::STRING, types::STRING}, {"pem_only_1", "pem_only_0"});
  EXPECT_THAT(*inserted_map->resolved_table_type(), IsTableType(expected_relation));

  EXPECT_THAT(inserted_map->Children(), ElementsAre(map2));
  auto expected_relation2 = Relation({types::STRING, types::STRING}, {"pem_only_0", "kelvin_only"});
  EXPECT_THAT(*map2->resolved_table_type(), IsTableType(expected_relation2));
}

TEST_F(SplitPEMAndKelvinOnlyUDFOperatorRuleTest, filter) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events", {"remote_addr", "req_path"});
  auto input1 = MakeColumn("remote_addr", 0);
  auto input2 = MakeColumn("req_path", 0);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {input2});
  auto func3 = MakeEqualsFunc(func1, func2);
  FilterIR* filter = MakeFilter(src1, func3);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  Relation existing_filter_relation({types::STRING, types::STRING}, {"remote_addr", "req_path"});
  EXPECT_THAT(*filter->resolved_table_type(), IsTableType(existing_filter_relation));

  SplitPEMAndKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], filter);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING, types::STRING, types::STRING},
                                 {"pem_only_0", "remote_addr", "req_path"});
  EXPECT_THAT(*new_map->resolved_table_type(), IsTableType(expected_map_relation));
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(filter));

  // original map relation and children shouldn't have changed.
  // pem_only func should now be a column projection.
  EXPECT_THAT(*filter->resolved_table_type(), IsTableType(existing_filter_relation));
  EXPECT_MATCH(filter->filter_expr(), Func());
  auto func = static_cast<FuncIR*>(filter->filter_expr());
  EXPECT_EQ("equal", func->func_name());
  EXPECT_EQ(2, func->args().size());
  EXPECT_MATCH(func->args()[0], ColumnNode("pem_only_0"));
  EXPECT_MATCH(func->args()[1], Func("kelvin_only"));
  EXPECT_THAT(filter->parents(), ElementsAre(new_map));
  EXPECT_THAT(filter->Children(), ElementsAre(sink));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
