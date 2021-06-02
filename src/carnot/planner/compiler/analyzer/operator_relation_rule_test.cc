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
#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/operator_relation_rule.h"
#include "src/carnot/planner/compiler/analyzer/setup_join_type_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAreArray;

class BlockingAggOperatorRelationRuleTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
  void SetUpGraph(bool resolve_agg_func, bool resolve_agg_group) {
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
    auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();

    auto agg_func = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                             std::vector<ExpressionIR*>{constant})
                        .ValueOrDie();
    if (resolve_agg_func) {
      agg_func->SetOutputDataType(func_data_type);
    }

    auto group = MakeColumn(group_name, /* parent_op_idx */ 0);
    // Code to resolve column.
    if (resolve_agg_group) {
      group->ResolveColumnType(group_data_type);
    }

    agg = graph
              ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{group},
                                          ColExpressionVector{{agg_func_col, agg_func}})
              .ValueOrDie();
  }
  MemorySourceIR* mem_src;
  BlockingAggIR* agg;
  types::DataType func_data_type = types::DataType::FLOAT64;
  types::DataType group_data_type = types::DataType::INT64;
  std::string group_name = "group";
  std::string agg_func_col = "meaned";
};

// Relation should resolve, all expressions in operator are resolved.
TEST_F(BlockingAggOperatorRelationRuleTest, successful_resolve) {
  SetUpGraph(true /* resolve_agg_func */, true /* resolve_agg_group */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(agg->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(agg->IsRelationInit());

  auto result_relation = agg->relation();
  table_store::schema::Relation expected_relation(
      {types::DataType::INT64, types::DataType::FLOAT64}, {group_name, agg_func_col});
  EXPECT_EQ(result_relation, expected_relation);
}
// Rule shouldn't work because column is not resolved.
TEST_F(BlockingAggOperatorRelationRuleTest, failed_resolve_column) {
  SetUpGraph(true /* resolve_agg_func */, false /* resolve_agg_group */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(agg->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  EXPECT_FALSE(agg->IsRelationInit());
}

// Rule shouldn't work because function is not resolved.
TEST_F(BlockingAggOperatorRelationRuleTest, failed_resolve_function) {
  SetUpGraph(false /* resolve_agg_func */, true /* resolve_agg_group */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(agg->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  EXPECT_FALSE(agg->IsRelationInit());
}
class MapOperatorRelationRuleTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
  void SetUpGraph(bool resolve_map_func, bool keep_input_columns) {
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
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
    if (resolve_map_func) {
      func_1->SetOutputDataType(func_data_type);
      func_2->SetOutputDataType(func_data_type);
    }

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
  SetUpGraph(true /* resolve_map_func */, false /* keep_input_columns */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(map->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(map->IsRelationInit());

  auto result_relation = map->relation();
  table_store::schema::Relation expected_relation({types::DataType::INT64, types::DataType::INT64},
                                                  {new_col_name, old_col_name});
  EXPECT_EQ(result_relation, expected_relation);
}

// Relation should resolve, all expressions in operator are resolved, and add the previous
// columns.
TEST_F(MapOperatorRelationRuleTest, successful_resolve_keep_input_columns) {
  SetUpGraph(true /* resolve_map_func */, true /* keep_input_columns */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(map->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(map->IsRelationInit());

  auto result_relation = map->relation();
  table_store::schema::Relation expected_relation(
      {types::DataType::INT64, types::DataType::FLOAT64, types::DataType::FLOAT64,
       types::DataType::INT64, types::DataType::INT64},
      {"count", "cpu1", "cpu2", new_col_name, old_col_name});
  EXPECT_EQ(result_relation, expected_relation);
}

// Rule shouldn't work because function is not resolved.
TEST_F(MapOperatorRelationRuleTest, failed_resolve_function) {
  SetUpGraph(false /* resolve_map_func */, false /* keep_input_columns */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(map->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  EXPECT_FALSE(map->IsRelationInit());
}

using UnionOperatorRelationRuleTest = RulesTest;
TEST_F(UnionOperatorRelationRuleTest, union_relation_setup) {
  auto mem_src1 = MakeMemSource(MakeRelation());
  auto mem_src2 = MakeMemSource(MakeRelation());
  auto union_op = MakeUnion({mem_src1, mem_src2});
  EXPECT_FALSE(union_op->IsRelationInit());

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(union_op->IsRelationInit());

  auto result_relation = union_op->relation();
  table_store::schema::Relation expected_relation = MakeRelation();
  EXPECT_THAT(result_relation.col_types(), ElementsAreArray(expected_relation.col_types()));
  EXPECT_THAT(result_relation.col_names(), ElementsAreArray(expected_relation.col_names()));

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
  auto mem_src1 = MakeMemSource(relation1);
  auto mem_src2 = MakeMemSource(relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});
  EXPECT_FALSE(union_op->IsRelationInit());
  EXPECT_TRUE(mem_src1->IsRelationInit());
  EXPECT_TRUE(mem_src2->IsRelationInit());

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
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
  auto mem_src1 = MakeMemSource(relation1);
  auto mem_src2 = MakeMemSource(relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});
  EXPECT_FALSE(union_op->IsRelationInit());
  EXPECT_TRUE(mem_src1->IsRelationInit());
  EXPECT_TRUE(mem_src2->IsRelationInit());

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);

  ASSERT_TRUE(union_op->IsRelationInit());
  Relation result_relation = union_op->relation();
  Relation expected_relation = relation1;
  EXPECT_THAT(result_relation.col_types(), ElementsAreArray(expected_relation.col_types()));
  EXPECT_THAT(result_relation.col_names(), ElementsAreArray(expected_relation.col_names()));

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

class OperatorRelationTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  LimitIR* MakeLimit(OperatorIR* parent) {
    return graph->CreateNode<LimitIR>(ast, parent, 10).ValueOrDie();
  }
  MemorySourceIR* mem_src;
};

// Make sure that relations are copied from node to node and the rule will execute consecutively.
TEST_F(OperatorRelationTest, propogate_test) {
  FilterIR* filter = MakeFilter(mem_src);
  LimitIR* limit = MakeLimit(filter);
  EXPECT_FALSE(filter->IsRelationInit());
  EXPECT_FALSE(limit->IsRelationInit());
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  bool did_change = false;
  do {
    auto result = op_rel_rule.Execute(graph.get());
    ASSERT_OK(result);
    did_change = result.ConsumeValueOrDie();
  } while (did_change);

  // Because limit comes after filter, it can actually evaluate in a single run.
  EXPECT_TRUE(filter->IsRelationInit());
  EXPECT_TRUE(limit->IsRelationInit());
}

TEST_F(OperatorRelationTest, mem_sink_with_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource(src_relation);
  MemorySinkIR* sink = MakeMemSink(src, "foo", {"cpu0"});

  OperatorRelationRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(Relation({types::DataType::FLOAT64}, {"cpu0"}), sink->relation());
}

TEST_F(OperatorRelationTest, mem_sink_all_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource(src_relation);
  MemorySinkIR* sink = MakeMemSink(src, "foo", {});

  OperatorRelationRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(src_relation, sink->relation());
}

TEST_F(OperatorRelationTest, grpc_sink_with_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource(src_relation);
  GRPCSinkIR* sink = MakeGRPCSink(src, "foo", {"cpu0"});

  OperatorRelationRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(Relation({types::DataType::FLOAT64}, {"cpu0"}), sink->relation());
}

TEST_F(OperatorRelationTest, grpc_sink_all_columns_test) {
  auto src_relation = MakeRelation();
  MemorySourceIR* src = MakeMemSource(src_relation);
  GRPCSinkIR* sink = MakeGRPCSink(src, "foo", {});

  OperatorRelationRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(src_relation, sink->relation());
}

TEST_F(OperatorRelationTest, JoinCreateOutputColumns) {
  std::string join_key = "key";
  Relation rel1({types::INT64, types::FLOAT64, types::STRING}, {join_key, "latency", "data"});
  Relation rel2({types::INT64, types::FLOAT64}, {join_key, "cpu_usage"});
  auto mem_src1 = MakeMemSource(rel1);
  auto mem_src2 = MakeMemSource(rel2);

  std::string left_suffix = "_x";
  std::string right_suffix = "_y";

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "inner", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_TRUE(mem_src1->IsRelationInit());
  EXPECT_TRUE(mem_src2->IsRelationInit());
  EXPECT_FALSE(join->IsRelationInit());

  EXPECT_EQ(join->output_columns().size(), 0);

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
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
  EXPECT_TRUE(join->IsRelationInit());
  EXPECT_EQ(join->relation(),
            Relation({types::INT64, types::FLOAT64, types::STRING, types::INT64, types::FLOAT64},
                     {"key_x", "latency", "data", "key_y", "cpu_usage"}));
}

TEST_F(OperatorRelationTest, JoinCreateOutputColumnsFailsDuplicateResultColumns) {
  std::string join_key = "key";
  std::string left_suffix = "_x";
  std::string right_suffix = "_y";
  std::string dup_key = absl::Substitute("$0$1", join_key, left_suffix);
  Relation rel1({types::INT64, types::FLOAT64, types::STRING}, {join_key, dup_key, "data"});
  Relation rel2({types::INT64, types::FLOAT64}, {join_key, "cpu_usage"});
  auto mem_src1 = MakeMemSource(rel1);
  auto mem_src2 = MakeMemSource(rel2);

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "inner", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_TRUE(mem_src1->IsRelationInit());
  EXPECT_TRUE(mem_src2->IsRelationInit());
  EXPECT_FALSE(join->IsRelationInit());

  EXPECT_EQ(join->output_columns().size(), 0);

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
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
  auto mem_src1 = MakeMemSource(rel1);
  auto mem_src2 = MakeMemSource(rel2);

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "inner", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_TRUE(mem_src1->IsRelationInit());
  EXPECT_TRUE(mem_src2->IsRelationInit());
  EXPECT_FALSE(join->IsRelationInit());

  EXPECT_EQ(join->output_columns().size(), 0);

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
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
  auto mem_src1 = MakeMemSource(rel1);
  auto mem_src2 = MakeMemSource(rel2);

  std::string left_suffix = "_x";
  std::string right_suffix = "_y";

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2},
                                          "right", std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();

  EXPECT_TRUE(mem_src1->IsRelationInit());
  EXPECT_TRUE(mem_src2->IsRelationInit());
  EXPECT_FALSE(join->IsRelationInit());

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

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
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
  EXPECT_TRUE(join->IsRelationInit());
  EXPECT_EQ(join->relation(),
            Relation({types::INT64, types::FLOAT64, types::STRING, types::INT64, types::FLOAT64},
                     {"key_x", "latency", "data", "key_y", "cpu_usage"}));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
