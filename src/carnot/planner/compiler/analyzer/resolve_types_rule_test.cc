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
#include <string>
#include <vector>

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/analyzer/setup_join_type_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAreArray;

using ResolveTypesRuleTest = RulesTest;
TEST_F(ResolveTypesRuleTest, map_then_agg) {
  auto mem_src = MakeMemSource("semantic_table", {"cpu", "bytes"});
  auto add_func = MakeAddFunc(MakeColumn("cpu", 0), MakeColumn("cpu", 0));
  auto add_func2 = MakeAddFunc(MakeColumn("bytes", 0), MakeColumn("bytes", 0));
  auto map = MakeMap(mem_src, {
                                  ColumnExpression("cpu_sum", add_func),
                                  ColumnExpression("bytes_sum", add_func2),
                              });
  auto mean_func = MakeMeanFunc(MakeColumn("cpu_sum", 0));
  auto mean_func2 = MakeMeanFunc(MakeColumn("bytes_sum", 0));
  auto agg = MakeBlockingAgg(map, /* groups */ {},
                             {
                                 ColumnExpression("cpu_sum_mean", mean_func),
                                 ColumnExpression("bytes_sum_mean", mean_func2),
                             });

  ResolveTypesRule types_rule(compiler_state_.get());
  auto result = types_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  auto map_type = std::static_pointer_cast<TableType>(map->resolved_type());
  auto agg_type = std::static_pointer_cast<TableType>(agg->resolved_type());

  // Add gets rid of ST_PERCENT but not ST_BYTES
  EXPECT_TableHasColumnWithType(map_type, "cpu_sum",
                                ValueType::Create(types::FLOAT64, types::ST_NONE))
      EXPECT_TableHasColumnWithType(map_type, "bytes_sum",
                                    ValueType::Create(types::INT64, types::ST_BYTES));
  EXPECT_TableHasColumnWithType(agg_type, "cpu_sum_mean",
                                ValueType::Create(types::FLOAT64, types::ST_NONE));
  // Note that mean turns Int->Float.
  EXPECT_TableHasColumnWithType(agg_type, "bytes_sum_mean",
                                ValueType::Create(types::FLOAT64, types::ST_BYTES));

  // The types rule shouldn't change anything after the first pass.
  result = types_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

// These tests used to live in OperatorRelationRule. They got copied over to try to maintain testing
// parity.
using OldOperatorRelationRuleTest = ResolveTypesRuleTest;

// Relation should resolve, all expressions in operator are resolved.
TEST_F(OldOperatorRelationRuleTest, successful_resolve) {
  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  compiler_state_->relation_map()->emplace("source", cpu_relation);
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();

  auto agg_func = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                           std::vector<ExpressionIR*>{constant})
                      .ValueOrDie();

  auto group = MakeColumn("cpu0", /* parent_op_idx */ 0);

  auto agg = graph
                 ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{group},
                                             ColExpressionVector{{"meaned", agg_func}})
                 .ValueOrDie();

  ResolveTypesRule type_rule(compiler_state_.get());
  EXPECT_FALSE(agg->is_type_resolved());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(agg->is_type_resolved());

  table_store::schema::Relation expected_relation(
      {types::DataType::FLOAT64, types::DataType::FLOAT64}, {"cpu0", "meaned"});
  EXPECT_THAT(*agg->resolved_table_type(), IsTableType(expected_relation));
}

class MapOperatorRelationRuleTest : public OldOperatorRelationRuleTest {
 protected:
  void SetUpGraph(bool keep_input_columns) {
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    compiler_state_->relation_map()->emplace("source", cpu_relation);
    auto constant1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
    auto constant2 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();

    auto func_1 = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                           std::vector<ExpressionIR*>{constant1, constant2})
                      .ValueOrDie();
    auto func_2 = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "*", "multiply"},
                                           std::vector<ExpressionIR*>{constant1, constant2})
                      .ValueOrDie();
    map = graph
              ->CreateNode<MapIR>(
                  ast, mem_src, ColExpressionVector{{new_col_name, func_1}, {old_col_name, func_2}},
                  keep_input_columns)
              .ValueOrDie();
  }
  MemorySourceIR* mem_src;
  MapIR* map;
  types::DataType func_data_type = types::DataType::INT64;
  std::string new_col_name = "sum";
  std::string old_col_name = "cpu0";
};

// Relation should resolve, all expressions in operator are resolved.
TEST_F(MapOperatorRelationRuleTest, successful_resolve) {
  SetUpGraph(false /* keep_input_columns */);
  ResolveTypesRule type_rule(compiler_state_.get());
  EXPECT_FALSE(map->is_type_resolved());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(map->is_type_resolved());

  table_store::schema::Relation expected_relation({types::DataType::INT64, types::DataType::INT64},
                                                  {new_col_name, old_col_name});
  EXPECT_THAT(*map->resolved_table_type(), IsTableType(expected_relation));
}

// Relation should resolve, all expressions in operator are resolved, and add the previous
// columns.
TEST_F(MapOperatorRelationRuleTest, successful_resolve_keep_input_columns) {
  SetUpGraph(true /* keep_input_columns */);
  ResolveTypesRule type_rule(compiler_state_.get());
  EXPECT_FALSE(map->is_type_resolved());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(map->is_type_resolved());

  table_store::schema::Relation expected_relation(
      {types::DataType::INT64, types::DataType::FLOAT64, types::DataType::FLOAT64,
       types::DataType::INT64, types::DataType::INT64},
      {"count", "cpu1", "cpu2", new_col_name, old_col_name});
  EXPECT_THAT(*map->resolved_table_type(), IsTableType(expected_relation));
}

using UnionOperatorRelationRuleTest = OldOperatorRelationRuleTest;
TEST_F(UnionOperatorRelationRuleTest, union_relation_setup) {
  auto rel = MakeRelation();
  compiler_state_->relation_map()->emplace("source", rel);
  auto mem_src1 = MakeMemSource("source", rel);
  auto mem_src2 = MakeMemSource("source", rel);
  auto union_op = MakeUnion({mem_src1, mem_src2});
  EXPECT_FALSE(union_op->is_type_resolved());

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(union_op->is_type_resolved());

  EXPECT_THAT(*union_op->resolved_table_type(), IsTableType(rel));

  EXPECT_EQ(union_op->column_mappings().size(), 2);
  std::vector<std::string> expected_names{"count", "cpu0", "cpu1", "cpu2"};
  auto actual_mappings_1 = union_op->column_mappings()[0];
  auto actual_mappings_2 = union_op->column_mappings()[1];

  EXPECT_EQ(actual_mappings_1.size(), expected_names.size());
  EXPECT_EQ(actual_mappings_2.size(), expected_names.size());

  for (const auto& [i, col] : Enumerate(actual_mappings_1)) {
    EXPECT_EQ(expected_names[i], col->col_name());
    EXPECT_EQ(0, col->container_op_parent_idx());
    EXPECT_EQ(mem_src1, col->ReferencedOperator().ConsumeValueOrDie());
  }
  for (const auto& [i, col] : Enumerate(actual_mappings_2)) {
    EXPECT_EQ(expected_names[i], col->col_name());
    EXPECT_EQ(1, col->container_op_parent_idx());
    EXPECT_EQ(mem_src2, col->ReferencedOperator().ConsumeValueOrDie());
  }
}

// Test whether the union disagreement fails with expected message.
TEST_F(UnionOperatorRelationRuleTest, union_relations_disagree) {
  Relation relation1 = MakeRelation();
  Relation relation2({types::DataType::INT64, types::DataType::FLOAT64}, {"count", "cpu0"});
  compiler_state_->relation_map()->emplace("source1", relation1);
  compiler_state_->relation_map()->emplace("source2", relation2);
  auto mem_src1 = MakeMemSource("source1", relation1);
  auto mem_src2 = MakeMemSource("source2", relation2);
  MakeUnion({mem_src1, mem_src2});

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_NOT_OK(result);
  auto memory_src_regex = "MemorySource\\([0-9A-z,=\\s]*\\)";
  EXPECT_THAT(result.status(),
              HasCompilerError("Table schema disagreement between parent ops $0 and "
                               "$0 of Union\\(id=[0-9]*\\). $0: \\[count:INT64, "
                               "cpu0:FLOAT64, cpu1:FLOAT64, "
                               "cpu2:FLOAT64\\] vs $0: \\[count:INT64, "
                               "cpu0:FLOAT64\\]. Column count wrong.",
                               memory_src_regex));

  skip_check_stray_nodes_ = true;
}

TEST_F(UnionOperatorRelationRuleTest, union_relation_different_order) {
  Relation relation1({types::DataType::TIME64NS, types::DataType::STRING, types::DataType::INT64},
                     {"time_", "strCol", "count"});
  Relation relation2({types::DataType::INT64, types::DataType::TIME64NS, types::DataType::STRING},
                     {"count", "time_", "strCol"});
  compiler_state_->relation_map()->emplace("source1", relation1);
  compiler_state_->relation_map()->emplace("source2", relation2);
  auto mem_src1 = MakeMemSource("source1", relation1);
  auto mem_src2 = MakeMemSource("source2", relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);

  ASSERT_TRUE(union_op->is_type_resolved());
  EXPECT_THAT(*union_op->resolved_table_type(), IsTableType(relation1));

  EXPECT_EQ(union_op->column_mappings().size(), 2);

  std::vector<std::string> expected_names{"time_", "strCol", "count"};
  auto actual_mappings_1 = union_op->column_mappings()[0];
  auto actual_mappings_2 = union_op->column_mappings()[1];

  EXPECT_EQ(actual_mappings_1.size(), expected_names.size());
  EXPECT_EQ(actual_mappings_2.size(), expected_names.size());

  for (const auto& [i, col] : Enumerate(actual_mappings_1)) {
    EXPECT_EQ(expected_names[i], col->col_name());
    EXPECT_EQ(0, col->container_op_parent_idx());
  }
  for (const auto& [i, col] : Enumerate(actual_mappings_2)) {
    EXPECT_EQ(expected_names[i], col->col_name());
    EXPECT_EQ(1, col->container_op_parent_idx());
  }
}

class OperatorRelationTest : public OldOperatorRelationRuleTest {
 protected:
  void SetUp() override {
    OldOperatorRelationRuleTest::SetUp();
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    compiler_state_->relation_map()->emplace("source", cpu_relation);
  }
  LimitIR* MakeLimit(OperatorIR* parent) {
    return graph->CreateNode<LimitIR>(ast, parent, 10).ValueOrDie();
  }
  MemorySourceIR* mem_src;
};

// Make sure that relations are copied from node to node and the rule will execute consecutively.
TEST_F(OperatorRelationTest, propogate_test) {
  auto constant1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto column = MakeColumn("count", 0);
  auto filter_func = graph
                         ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equal"},
                                              std::vector<ExpressionIR*>{constant1, column})
                         .ValueOrDie();
  auto filter = graph->CreateNode<FilterIR>(ast, mem_src, filter_func).ValueOrDie();
  LimitIR* limit = MakeLimit(filter);
  EXPECT_FALSE(filter->is_type_resolved());
  EXPECT_FALSE(limit->is_type_resolved());
  ResolveTypesRule type_rule(compiler_state_.get());
  bool did_change = false;
  do {
    auto result = type_rule.Execute(graph.get());
    ASSERT_OK(result);
    did_change = result.ConsumeValueOrDie();
  } while (did_change);

  // Because limit comes after filter, it can actually evaluate in a single run.
  EXPECT_TRUE(filter->is_type_resolved());
  EXPECT_TRUE(limit->is_type_resolved());
}

TEST_F(OperatorRelationTest, mem_sink_with_columns_test) {
  auto src_relation = MakeRelation();
  compiler_state_->relation_map()->emplace("source", src_relation);
  MemorySourceIR* src = MakeMemSource("source", src_relation);
  MemorySinkIR* sink = MakeMemSink(src, "foo", {"cpu0"});

  ResolveTypesRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(*sink->resolved_table_type(),
              IsTableType(Relation({types::DataType::FLOAT64}, {"cpu0"})));
}

TEST_F(OperatorRelationTest, mem_sink_all_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource("source", src_relation);
  compiler_state_->relation_map()->emplace("source", src_relation);
  MemorySinkIR* sink = MakeMemSink(src, "foo", {});

  ResolveTypesRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(src_relation));
}

TEST_F(OperatorRelationTest, grpc_sink_with_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource("source", src_relation);
  compiler_state_->relation_map()->emplace("source", src_relation);
  GRPCSinkIR* sink = MakeGRPCSink(src, "foo", {"cpu0"});

  ResolveTypesRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(*sink->resolved_table_type(),
              IsTableType(Relation({types::DataType::FLOAT64}, {"cpu0"})));
}

TEST_F(OperatorRelationTest, grpc_sink_all_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource("source", src_relation);
  compiler_state_->relation_map()->emplace("source", src_relation);
  GRPCSinkIR* sink = MakeGRPCSink(src, "foo", {});

  ResolveTypesRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(*sink->resolved_table_type(), IsTableType(src_relation));
}

TEST_F(OperatorRelationTest, JoinCreateOutputColumns) {
  std::string join_key = "key";
  Relation rel1({types::INT64, types::FLOAT64, types::STRING}, {join_key, "latency", "data"});
  Relation rel2({types::INT64, types::FLOAT64}, {join_key, "cpu_usage"});
  auto mem_src1 = MakeMemSource("table1", rel1);
  compiler_state_->relation_map()->emplace("table1", rel1);
  auto mem_src2 = MakeMemSource("table2", rel2);
  compiler_state_->relation_map()->emplace("table2", rel2);

  std::string left_suffix = "_x";
  std::string right_suffix = "_y";

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "inner", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_EQ(join->output_columns().size(), 0);

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // Check that output columns are named what we expect.
  EXPECT_EQ(join->output_columns().size(), 5);
  EXPECT_TRUE(Match(join->output_columns()[0], ColumnNode(join_key, /* parent_idx */ 0)))
      << join->output_columns()[0]->DebugString();
  EXPECT_MATCH(join->output_columns()[1], ColumnNode("latency", /* parent_idx */ 0));
  EXPECT_MATCH(join->output_columns()[2], ColumnNode("data", /* parent_idx */ 0));
  EXPECT_MATCH(join->output_columns()[3], ColumnNode(join_key, /* parent_idx */ 1));
  EXPECT_MATCH(join->output_columns()[4], ColumnNode("cpu_usage", /* parent_idx */ 1));

  // Match expected data types.
  EXPECT_MATCH(join->output_columns()[0], Expression(types::INT64));
  EXPECT_MATCH(join->output_columns()[1], Expression(types::FLOAT64));
  EXPECT_MATCH(join->output_columns()[2], Expression(types::STRING));
  EXPECT_MATCH(join->output_columns()[3], Expression(types::INT64));
  EXPECT_MATCH(join->output_columns()[4], Expression(types::FLOAT64));

  // Join relation should be set.
  EXPECT_TRUE(join->is_type_resolved());
  EXPECT_THAT(*join->resolved_table_type(),
              IsTableType(Relation(
                  {types::INT64, types::FLOAT64, types::STRING, types::INT64, types::FLOAT64},
                  {"key_x", "latency", "data", "key_y", "cpu_usage"})));
}

TEST_F(OperatorRelationTest, JoinCreateOutputColumnsFailsDuplicateResultColumns) {
  std::string join_key = "key";
  std::string left_suffix = "_x";
  std::string right_suffix = "_y";
  std::string dup_key = absl::Substitute("$0$1", join_key, left_suffix);
  Relation rel1({types::INT64, types::FLOAT64, types::STRING}, {join_key, dup_key, "data"});
  Relation rel2({types::INT64, types::FLOAT64}, {join_key, "cpu_usage"});
  auto mem_src1 = MakeMemSource("table1", rel1);
  compiler_state_->relation_map()->emplace("table1", rel1);
  auto mem_src2 = MakeMemSource("table2", rel2);
  compiler_state_->relation_map()->emplace("table2", rel2);

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "inner", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_EQ(join->output_columns().size(), 0);

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_NOT_OK(result);
  EXPECT_THAT(result.status(), HasCompilerError("duplicate column '$0' after merge. Change the "
                                                "specified suffixes .*'$1','$2'.* to fix this",
                                                dup_key, left_suffix, right_suffix));
}

TEST_F(OperatorRelationTest, JoinCreateOutputColumnsFailsDuplicateNoSuffixes) {
  std::string join_key = "key";
  std::string left_suffix = "";
  std::string right_suffix = "";
  Relation rel1({types::INT64, types::FLOAT64, types::STRING}, {join_key, "latency_ns", "data"});
  Relation rel2({types::INT64, types::FLOAT64}, {join_key, "cpu_usage"});
  auto mem_src1 = MakeMemSource("table1", rel1);
  compiler_state_->relation_map()->emplace("table1", rel1);
  auto mem_src2 = MakeMemSource("table2", rel2);
  compiler_state_->relation_map()->emplace("table2", rel2);

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "inner", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_EQ(join->output_columns().size(), 0);

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_NOT_OK(result);
  EXPECT_THAT(result.status(),
              HasCompilerError("duplicate column '$0' after merge. Change the specified suffixes.*",
                               join_key));
}

// The right join is a weird special case for output columns - we need the order of the output
// columns to be the same -> this ensures that.
TEST_F(OperatorRelationTest, JoinCreateOutputColumnsAfterRightJoin) {
  std::string join_key = "key";
  Relation rel1({types::INT64, types::FLOAT64, types::STRING}, {join_key, "latency", "data"});
  Relation rel2({types::INT64, types::FLOAT64}, {join_key, "cpu_usage"});
  auto mem_src1 = MakeMemSource("table1", rel1);
  compiler_state_->relation_map()->emplace("table1", rel1);
  auto mem_src2 = MakeMemSource("table2", rel2);
  compiler_state_->relation_map()->emplace("table2", rel2);

  std::string left_suffix = "_x";
  std::string right_suffix = "_y";

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "right", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();
  EXPECT_EQ(join->output_columns().size(), 0);

  // Join should be a right join.
  EXPECT_TRUE(join->specified_as_right());
  EXPECT_TRUE(join->join_type() == JoinIR::JoinType::kRight);

  // Converts right join to left join.
  SetupJoinTypeRule rule;
  auto result_or_s = rule.Execute(graph.get());
  ASSERT_OK(result_or_s);
  EXPECT_TRUE(result_or_s.ValueOrDie());

  // Join should still be specified as a  right join.
  EXPECT_TRUE(join->specified_as_right());
  // But this switches over as internally Left is a simple column reshuffling of a Right join.
  EXPECT_TRUE(join->join_type() == JoinIR::JoinType::kLeft);

  ResolveTypesRule type_rule(compiler_state_.get());
  auto result = type_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // Check that output columns are named what we expect.
  EXPECT_EQ(join->output_columns().size(), 5);
  EXPECT_TRUE(Match(join->output_columns()[0], ColumnNode(join_key, /* parent_idx */ 1)))
      << join->output_columns()[0]->DebugString();
  EXPECT_MATCH(join->output_columns()[1], ColumnNode("latency", /* parent_idx */ 1));
  EXPECT_MATCH(join->output_columns()[2], ColumnNode("data", /* parent_idx */ 1));
  EXPECT_MATCH(join->output_columns()[3], ColumnNode(join_key, /* parent_idx */ 0));
  EXPECT_MATCH(join->output_columns()[4], ColumnNode("cpu_usage", /* parent_idx */ 0));

  // Match expected data types.
  EXPECT_MATCH(join->output_columns()[0], Expression(types::INT64));
  EXPECT_MATCH(join->output_columns()[1], Expression(types::FLOAT64));
  EXPECT_MATCH(join->output_columns()[2], Expression(types::STRING));
  EXPECT_MATCH(join->output_columns()[3], Expression(types::INT64));
  EXPECT_MATCH(join->output_columns()[4], Expression(types::FLOAT64));

  // Join relation should be set.
  EXPECT_TRUE(join->is_type_resolved());
  EXPECT_THAT(*join->resolved_table_type(),
              IsTableType(Relation(
                  {types::INT64, types::FLOAT64, types::STRING, types::INT64, types::FLOAT64},
                  {"key_x", "latency", "data", "key_y", "cpu_usage"})));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
