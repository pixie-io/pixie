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

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/optimizer/prune_unused_columns_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::ElementsAre;

using PruneUnusedColumnsRuleTest = RulesTest;

TEST_F(PruneUnusedColumnsRuleTest, basic) {
  auto relation = MakeRelation();
  MemorySourceIR* mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2}, false);
  Relation map_relation{{types::DataType::INT64, types::DataType::FLOAT64}, {"count_1", "cpu0_1"}};

  auto sink = MakeMemSink(map, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(*mem_src->resolved_table_type(),
              IsTableType(Relation({types::DataType::FLOAT64}, {"cpu0"})));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("cpu0"));

  EXPECT_THAT(*map->resolved_table_type(), IsTableType(sink_relation));
  EXPECT_EQ(1, map->col_exprs().size());
  EXPECT_EQ(expr2.name, map->col_exprs()[0].name);
  EXPECT_EQ(expr2.node, map->col_exprs()[0].node);

  // Should be unchanged
  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(sink_relation));
}

TEST_F(PruneUnusedColumnsRuleTest, filter) {
  auto relation = MakeRelation();
  MemorySourceIR* mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2}, false);
  Relation map_relation{{types::DataType::INT64, types::DataType::FLOAT64}, {"count_1", "cpu0_1"}};

  auto eq_func = MakeEqualsFunc(MakeColumn("count_1", 0), MakeColumn("cpu0_1", 0));
  auto filter = MakeFilter(map, eq_func);

  auto sink = MakeMemSink(filter, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(*mem_src->resolved_table_type(),
              IsTableType(Relation({types::DataType::INT64, types::DataType::FLOAT64}, {
                                                                                           "count",
                                                                                           "cpu0",
                                                                                       })));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("count", "cpu0"));

  EXPECT_THAT(*map->resolved_table_type(), IsTableType(map_relation));
  EXPECT_EQ(2, map->col_exprs().size());
  EXPECT_EQ(expr1.name, map->col_exprs()[0].name);
  EXPECT_EQ(expr1.node, map->col_exprs()[0].node);
  EXPECT_EQ(expr2.name, map->col_exprs()[1].name);
  EXPECT_EQ(expr2.node, map->col_exprs()[1].node);

  // Should be unchanged
  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(sink_relation));
}

TEST_F(PruneUnusedColumnsRuleTest, two_filters) {
  auto relation = MakeRelation();
  MemorySourceIR* mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);

  auto eq_func = MakeEqualsFunc(MakeColumn("count", 0), MakeInt(10));
  auto eq_func2 = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeColumn("cpu1", 0));
  auto filter1 = MakeFilter(mem_src, eq_func);
  auto filter2 = MakeFilter(filter1, eq_func2);

  auto sink = MakeMemSink(filter2, "abc", {"cpu2"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu2"}};

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(mem_src->resolved_table_type()->ColumnNames(),
              ElementsAre("count", "cpu0", "cpu1", "cpu2"));
  EXPECT_THAT(filter1->resolved_table_type()->ColumnNames(), ElementsAre("cpu0", "cpu1", "cpu2"));
  EXPECT_THAT(filter2->resolved_table_type()->ColumnNames(), ElementsAre("cpu2"));

  // Should be unchanged
  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(sink_relation));
}

TEST_F(PruneUnusedColumnsRuleTest, multiparent) {
  Relation relation0{{types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"}};
  auto mem_src1 = MakeMemSource("source0", relation0);
  compiler_state_->relation_map()->emplace("source0", relation0);

  Relation relation1{{types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"}};
  auto mem_src2 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);

  auto join_op =
      MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
               std::vector<std::string>{"col1"}, std::vector<std::string>{"col2"}, {"_left", ""});

  std::vector<std::string> join_out_cols{"left_only", "col1_left", "right_only", "col2_right"};
  Relation join_relation{{types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                          types::DataType::INT64},
                         join_out_cols};

  std::vector<std::string> sink_out_cols{"right_only", "col1_left"};
  auto sink = MakeMemSink(join_op, "abc", sink_out_cols);
  Relation sink_relation{{types::DataType::INT64, types::DataType::INT64}, sink_out_cols};

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  // Check mem sources
  Relation mem_src1_relation{{types::DataType::INT64}, {"col1"}};
  EXPECT_THAT(*mem_src1->resolved_table_type(), IsTableType(mem_src1_relation));
  EXPECT_THAT(mem_src1->column_names(), ElementsAre("col1"));

  Relation mem_src2_relation{{types::DataType::INT64, types::DataType::INT64},
                             {"right_only", "col2"}};
  EXPECT_THAT(*mem_src2->resolved_table_type(), IsTableType(mem_src2_relation));
  EXPECT_THAT(mem_src2->column_names(), ElementsAre("right_only", "col2"));

  // Check join
  Relation new_join_relation{{types::DataType::INT64, types::DataType::INT64},
                             {"col1_left", "right_only"}};
  EXPECT_THAT(*join_op->resolved_table_type(), IsTableType(new_join_relation));
  EXPECT_EQ(2, join_op->output_columns().size());
  EXPECT_EQ("col1", join_op->output_columns()[0]->col_name());
  EXPECT_EQ(0, join_op->output_columns()[0]->container_op_parent_idx());
  EXPECT_EQ("right_only", join_op->output_columns()[1]->col_name());
  EXPECT_EQ(1, join_op->output_columns()[1]->container_op_parent_idx());
  EXPECT_THAT(join_op->column_names(), ElementsAre("col1_left", "right_only"));

  // Check mem sink, should be unchanged
  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(sink_relation));
}

TEST_F(PruneUnusedColumnsRuleTest, unchanged) {
  auto rel = MakeRelation();
  MemorySourceIR* mem_src = MakeMemSource("source", rel);
  compiler_state_->relation_map()->emplace("source", rel);

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};
  ColumnExpression expr3{"cpu1_1", MakeColumn("cpu1", 0)};
  ColumnExpression expr4{"cpu2_1", MakeColumn("cpu2", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2, expr3, expr4}, false);
  std::vector<std::string> out_cols{"count_1", "cpu0_1", "cpu1_1", "cpu2_1"};
  Relation relation{{types::DataType::INT64, types::DataType::FLOAT64, types::DataType::FLOAT64,
                     types::DataType::FLOAT64},
                    out_cols};

  MakeMemSink(map, "abc", out_cols);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());
}

TEST_F(PruneUnusedColumnsRuleTest, updates_resolved_type) {
  auto relation = MakeRelation();
  MemorySourceIR* mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2}, false);
  Relation map_relation{{types::DataType::INT64, types::DataType::FLOAT64}, {"count_1", "cpu0_1"}};

  auto sink = MakeMemSink(map, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(*mem_src->resolved_table_type(),
              IsTableType(Relation({types::DataType::FLOAT64}, {"cpu0"})));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("cpu0"));
  auto new_src_table_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  EXPECT_TRUE(new_src_table_type->HasColumn("cpu0"));
  EXPECT_FALSE(new_src_table_type->HasColumn("count"));
  EXPECT_EQ(*ValueType::Create(types::FLOAT64, types::ST_NONE),
            *std::static_pointer_cast<ValueType>(
                new_src_table_type->GetColumnType("cpu0").ConsumeValueOrDie()));

  EXPECT_THAT(*map->resolved_table_type(), IsTableType(sink_relation));
  EXPECT_EQ(1, map->col_exprs().size());
  EXPECT_EQ(expr2.name, map->col_exprs()[0].name);
  EXPECT_EQ(expr2.node, map->col_exprs()[0].node);

  auto new_map_table_type = std::static_pointer_cast<TableType>(map->resolved_type());
  EXPECT_TRUE(new_map_table_type->HasColumn("cpu0_1"));
  EXPECT_EQ(*ValueType::Create(types::DataType::FLOAT64, types::ST_NONE),
            *std::static_pointer_cast<ValueType>(
                new_map_table_type->GetColumnType("cpu0_1").ConsumeValueOrDie()));
  EXPECT_FALSE(new_map_table_type->HasColumn("count_1"));

  // Should be unchanged
  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(sink_relation));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
