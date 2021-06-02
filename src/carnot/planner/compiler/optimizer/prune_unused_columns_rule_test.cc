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
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2}, false);
  Relation map_relation{{types::DataType::INT64, types::DataType::FLOAT64}, {"count_1", "cpu0_1"}};
  ASSERT_OK(map->SetRelation(map_relation));

  auto sink = MakeMemSink(map, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};
  ASSERT_OK(sink->SetRelation(sink_relation));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_EQ(mem_src->relation(), Relation({types::DataType::FLOAT64}, {"cpu0"}));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("cpu0"));

  EXPECT_EQ(map->relation(), sink_relation);
  EXPECT_EQ(1, map->col_exprs().size());
  EXPECT_EQ(expr2.name, map->col_exprs()[0].name);
  EXPECT_EQ(expr2.node, map->col_exprs()[0].node);

  // Should be unchanged
  EXPECT_EQ(sink_relation, sink->relation());
}

TEST_F(PruneUnusedColumnsRuleTest, filter) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2}, false);
  Relation map_relation{{types::DataType::INT64, types::DataType::FLOAT64}, {"count_1", "cpu0_1"}};
  ASSERT_OK(map->SetRelation(map_relation));

  auto filter = MakeFilter(map, MakeEqualsFunc(MakeColumn("count_1", 0), MakeColumn("cpu0_1", 0)));
  ASSERT_OK(filter->SetRelation(map_relation));

  auto sink = MakeMemSink(filter, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};
  ASSERT_OK(sink->SetRelation(sink_relation));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_EQ(mem_src->relation(),
            Relation({types::DataType::INT64, types::DataType::FLOAT64}, {
                                                                             "count",
                                                                             "cpu0",
                                                                         }));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("count", "cpu0"));

  EXPECT_EQ(map_relation, map->relation());
  EXPECT_EQ(2, map->col_exprs().size());
  EXPECT_EQ(expr1.name, map->col_exprs()[0].name);
  EXPECT_EQ(expr1.node, map->col_exprs()[0].node);
  EXPECT_EQ(expr2.name, map->col_exprs()[1].name);
  EXPECT_EQ(expr2.node, map->col_exprs()[1].node);

  // Should be unchanged
  EXPECT_EQ(sink_relation, sink->relation());
}

TEST_F(PruneUnusedColumnsRuleTest, two_filters) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());

  auto filter1 = MakeFilter(mem_src, MakeEqualsFunc(MakeColumn("count", 0), MakeInt(10)));
  ASSERT_OK(filter1->SetRelation(MakeRelation()));
  auto filter2 = MakeFilter(filter1, MakeEqualsFunc(MakeColumn("cpu0", 0), MakeColumn("cpu1", 0)));
  ASSERT_OK(filter2->SetRelation(MakeRelation()));

  auto sink = MakeMemSink(filter2, "abc", {"cpu2"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu2"}};
  ASSERT_OK(sink->SetRelation(sink_relation));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(mem_src->relation().col_names(), ElementsAre("count", "cpu0", "cpu1", "cpu2"));
  EXPECT_THAT(filter1->relation().col_names(), ElementsAre("cpu0", "cpu1", "cpu2"));
  EXPECT_THAT(filter2->relation().col_names(), ElementsAre("cpu2"));

  // Should be unchanged
  EXPECT_EQ(sink_relation, sink->relation());
}

TEST_F(PruneUnusedColumnsRuleTest, multiparent) {
  Relation relation0{{types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"}};
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1{{types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"}};
  auto mem_src2 = MakeMemSource(relation1);

  auto join_op = MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
                          std::vector<std::string>{"col1"}, std::vector<std::string>{"col2"});

  std::vector<std::string> join_out_cols{"right_only", "col2_right", "left_only", "col1_left"};
  ASSERT_OK(join_op->SetOutputColumns(join_out_cols,
                                      {MakeColumn("right_only", 1), MakeColumn("col2", 1),
                                       MakeColumn("left_only", 0), MakeColumn("col1", 0)}));
  Relation join_relation{{types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                          types::DataType::INT64},
                         join_out_cols};
  ASSERT_OK(join_op->SetRelation(join_relation));

  std::vector<std::string> sink_out_cols{"right_only", "col1_left"};
  auto sink = MakeMemSink(join_op, "abc", sink_out_cols);
  Relation sink_relation{{types::DataType::INT64, types::DataType::INT64}, sink_out_cols};
  ASSERT_OK(sink->SetRelation(sink_relation));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  // Check mem sources
  Relation mem_src1_relation{{types::DataType::INT64}, {"col1"}};
  EXPECT_EQ(mem_src1_relation, mem_src1->relation());
  EXPECT_THAT(mem_src1->column_names(), ElementsAre("col1"));

  Relation mem_src2_relation{{types::DataType::INT64, types::DataType::INT64},
                             {"right_only", "col2"}};
  EXPECT_EQ(mem_src2_relation, mem_src2->relation());
  EXPECT_THAT(mem_src2->column_names(), ElementsAre("right_only", "col2"));

  // Check join
  Relation new_join_relation{{types::DataType::INT64, types::DataType::INT64},
                             {"right_only", "col1_left"}};
  EXPECT_EQ(new_join_relation, join_op->relation());
  EXPECT_EQ(2, join_op->output_columns().size());
  EXPECT_EQ("right_only", join_op->output_columns()[0]->col_name());
  EXPECT_EQ(1, join_op->output_columns()[0]->container_op_parent_idx());
  EXPECT_EQ("col1", join_op->output_columns()[1]->col_name());
  EXPECT_EQ(0, join_op->output_columns()[1]->container_op_parent_idx());
  EXPECT_THAT(join_op->column_names(), ElementsAre("right_only", "col1_left"));

  // Check mem sink, should be unchanged
  EXPECT_EQ(sink_relation, sink->relation());
}

TEST_F(PruneUnusedColumnsRuleTest, unchanged) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};
  ColumnExpression expr3{"cpu1_1", MakeColumn("cpu1", 0)};
  ColumnExpression expr4{"cpu2_1", MakeColumn("cpu2", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2, expr3, expr4}, false);
  std::vector<std::string> out_cols{"count_1", "cpu0_1", "cpu1_1", "cpu2_1"};
  Relation relation{{types::DataType::INT64, types::DataType::FLOAT64, types::DataType::FLOAT64,
                     types::DataType::FLOAT64},
                    out_cols};
  ASSERT_OK(map->SetRelation(relation));

  auto sink = MakeMemSink(map, "abc", out_cols);
  ASSERT_OK(sink->SetRelation(relation));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());
}

TEST_F(PruneUnusedColumnsRuleTest, updates_resolved_type) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());
  auto mem_src_type = TableType::Create(mem_src->relation());
  ASSERT_OK(mem_src->SetResolvedType(mem_src_type));

  ColumnExpression expr1{"count_1", MakeColumn("count", 0)};
  ColumnExpression expr2{"cpu0_1", MakeColumn("cpu0", 0)};

  auto map = MakeMap(mem_src, {expr1, expr2}, false);
  Relation map_relation{{types::DataType::INT64, types::DataType::FLOAT64}, {"count_1", "cpu0_1"}};
  ASSERT_OK(map->SetRelation(map_relation));
  ASSERT_OK(ResolveOperatorType(map, compiler_state_.get()));

  auto sink = MakeMemSink(map, "abc", {"cpu0_1"});
  Relation sink_relation{{types::DataType::FLOAT64}, {"cpu0_1"}};
  ASSERT_OK(sink->SetRelation(sink_relation));
  ASSERT_OK(ResolveOperatorType(sink, compiler_state_.get()));

  PruneUnusedColumnsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_EQ(mem_src->relation(), Relation({types::DataType::FLOAT64}, {"cpu0"}));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("cpu0"));
  auto new_src_table_type = std::static_pointer_cast<TableType>(mem_src->resolved_type());
  EXPECT_TRUE(new_src_table_type->HasColumn("cpu0"));
  EXPECT_FALSE(new_src_table_type->HasColumn("count"));
  EXPECT_EQ(*ValueType::Create(types::FLOAT64, types::ST_NONE),
            *std::static_pointer_cast<ValueType>(
                new_src_table_type->GetColumnType("cpu0").ConsumeValueOrDie()));

  EXPECT_EQ(map->relation(), sink_relation);
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
  EXPECT_EQ(sink_relation, sink->relation());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
