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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/parser/parser.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::_;

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

class AnalyzerTest : public ASTVisitorTest {
 protected:
  Status HandleRelation(std::shared_ptr<IR> ir_graph) {
    PX_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer,
                        Analyzer::Create(compiler_state_.get()));
    return analyzer->Execute(ir_graph.get());
  }
};

TEST_F(AnalyzerTest, test_utils) {
  table_store::schema::Relation cpu2_relation;
  cpu2_relation.AddColumn(types::FLOAT64, "cpu0");
  cpu2_relation.AddColumn(types::FLOAT64, "cpu1");
  EXPECT_THAT((*compiler_state_->relation_map())["cpu"],
              Not(UnorderedRelationMatches(cpu2_relation)));
  EXPECT_EQ((*compiler_state_->relation_map())["cpu"], (*compiler_state_->relation_map())["cpu"]);
}

TEST_F(AnalyzerTest, no_special_relation) {
  std::string from_expr =
      "import px\ndf = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])\npx.display(df, 'cpu')";
  auto ir_graph_status = CompileGraph(from_expr);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  // check the connection of ig
  std::string from_range_expr =
      "import px\ndf px.DataFrame(table='cpu', select=['cpu0'], start_time=0, "
      "end_time=10)\npx.display(df)";
  ir_graph_status = CompileGraph(from_expr);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(AnalyzerTest, assign_functionality) {
  std::string assign_and_use = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
       "px.display(queryDF, 'cpu_out')"},
      "\n");

  auto ir_graph_status = CompileGraph(assign_and_use);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

// Map Tests
TEST_F(AnalyzerTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "df = queryDF[['sum']]",
       "px.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(single_col_map_sum);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_div_map_query = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['div'] = queryDF['cpu0'] / queryDF['cpu1']", "df = queryDF[['div']]",
       "px.display(df, 'cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(single_col_div_map_query);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(AnalyzerTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', "
          "'cpu2'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "queryDF['copy'] = queryDF['cpu2']",
          "df = queryDF[['sum', 'copy']]",
          "px.display(df, 'cpu_out')",
      },
      "\n");
  auto ir_graph_status = CompileGraph(multi_col);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(AnalyzerTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "df = queryDF[['sum']]",
       "px.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(single_col_map_sum);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_sub = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']", "df = queryDF[['sub']]",
       "px.display(df, 'cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(single_col_map_sub);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_product = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['product'] = queryDF['cpu0'] * queryDF['cpu1']", "df = queryDF[['product']]",
       "px.display(df, 'cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(single_col_map_product);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_quotient = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['quotient'] = queryDF['cpu0'] / queryDF['cpu1']", "df = queryDF[['quotient']]",
       "px.display(df, 'cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(single_col_map_quotient);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(AnalyzerTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "df = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count))",
       "px.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(single_col_agg);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);

  std::string multi_output_col_agg = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0','cpu1'], start_time=0, end_time=10)",
       "df = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count),",
       "cpu_mean=('cpu1', px.mean))", "px.display(df)"},
      "\n");
  ir_graph_status = CompileGraph(multi_output_col_agg);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
}

// Make sure the relations match the expected values.
TEST_F(AnalyzerTest, test_relation_results) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['upid', 'cpu0', 'cpu1', "
       "'cpu2', 'agent_id'], start_time=0, end_time=10)",
       "px.display(queryDF)", "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
       "groupDF = queryDF[['cpu0', 'cpu1', 'cpu_sum']].groupby('cpu0')",
       "df = groupDF.agg(cpu_count=('cpu1', px.count),cpu_mean=('cpu1', px.mean))",
       "px.display(df)"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  ASSERT_OK(handle_status);

  // Memory Source should copy the source relation.
  std::vector<IRNode*> source_nodes = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  EXPECT_EQ(source_nodes.size(), 1);
  auto source_node = static_cast<MemorySourceIR*>(source_nodes[0]);
  EXPECT_THAT(
      *source_node->resolved_table_type(),
      IsTableType(std::vector<types::DataType>{types::UINT128, types::FLOAT64, types::FLOAT64,
                                               types::FLOAT64, types::INT64},
                  std::vector<std::string>{"upid", "cpu0", "cpu1", "cpu2", "agent_id"}));

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  std::vector<IRNode*> map_nodes = ir_graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 2);

  Relation sum_relation({types::UINT128, types::FLOAT64, types::FLOAT64, types::FLOAT64,
                         types::INT64, types::FLOAT64},
                        {"upid", "cpu0", "cpu1", "cpu2", "agent_id", "cpu_sum"});
  Relation drop_relation({types::FLOAT64, types::FLOAT64, types::FLOAT64},
                         {"cpu0", "cpu1", "cpu_sum"});
  std::vector<TableType> map_types{*static_cast<MapIR*>(map_nodes[0])->resolved_table_type(),
                                   *static_cast<MapIR*>(map_nodes[1])->resolved_table_type()};
  EXPECT_THAT(map_types,
              UnorderedElementsAre(IsTableType(sum_relation), IsTableType(drop_relation)));

  // Agg should be a new relation with one column.
  std::vector<IRNode*> agg_nodes = ir_graph->FindNodesOfType(IRNodeType::kBlockingAgg);
  EXPECT_EQ(agg_nodes.size(), 1);
  auto agg_node = static_cast<BlockingAggIR*>(agg_nodes[0]);
  table_store::schema::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  EXPECT_THAT(*agg_node->resolved_table_type(), IsTableType(test_agg_relation));

  EXPECT_EQ(agg_node->Children().size(), 1);
  ASSERT_MATCH(agg_node->Children()[0], Limit(10000));
  auto limit = static_cast<LimitIR*>(agg_node->Children()[0]);
  EXPECT_THAT(*limit->resolved_table_type(), IsTableType(test_agg_relation));
  ASSERT_MATCH(limit->Children()[0], ExternalGRPCSink());
  auto sink_node = static_cast<GRPCSinkIR*>(limit->Children()[0]);
  EXPECT_THAT(*sink_node->resolved_table_type(), IsTableType(test_agg_relation));
}

// Make sure the compiler exits when calling columns that aren't explicitly called.
TEST_F(AnalyzerTest, test_relation_fails) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], start_time=0, "
       "end_time=10)",
       "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']", "queryDF = queryDF[['cpu_sum']]",
       // Trying to group by cpu0 which was dropped.
       "df = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count),",
       "cpu_mean=('cpu1', px.mean))", "px.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();

  // This query assumes implicit copying of Input relation into Map. The relation handler should
  // fail.
  auto handle_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(handle_status);

  EXPECT_THAT(handle_status, HasCompilerError("Column 'cpu[0-9]' not found in parent dataframe"));
}

TEST_F(AnalyzerTest, test_relation_multi_col_agg) {
  std::string chain_operators =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby(['cpu0', 'cpu2']).agg(cpu_count=('cpu1', px.count),",
                     "cpu_mean=('cpu1', px.mean))", "px.display(aggDF)"},
                    "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  VLOG(1) << handle_status.ToString();
  ASSERT_OK(handle_status);

  std::vector<IRNode*> agg_nodes = ir_graph->FindNodesOfType(IRNodeType::kBlockingAgg);
  EXPECT_EQ(agg_nodes.size(), 1);
  auto agg_node = static_cast<BlockingAggIR*>(agg_nodes[0]);
  table_store::schema::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu2");
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  EXPECT_THAT(*agg_node->resolved_table_type(), IsTableType(test_agg_relation));
}

TEST_F(AnalyzerTest, test_from_select) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators =
      "import px\nqueryDF = px.DataFrame(table='cpu', select=['cpu0', "
      "'cpu2'], start_time=0, end_time=10)\npx.display(queryDF)";
  table_store::schema::Relation test_relation;
  test_relation.AddColumn(types::FLOAT64, "cpu0");
  test_relation.AddColumn(types::FLOAT64, "cpu2");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  std::vector<IRNode*> sink_nodes = ir_graph->FindNodesThatMatch(ExternalGRPCSink());
  EXPECT_EQ(sink_nodes.size(), 1);
  auto sink_node = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  EXPECT_THAT(*sink_node->resolved_table_type(), IsTableType(test_relation));
}

TEST_F(AnalyzerTest, nonexistent_cols) {
  // Test for columns used in map function that don't exist in relation.
  std::string wrong_column_map_func = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['cpu_sum'] = px.sum(queryDF['cpu0'], queryDF['cpu100'])",
       "df = queryDF[['cpu_sum']]", "px.display(df, 'cpu_out')"},
      "\n");

  auto ir_graph_status = CompileGraph(wrong_column_map_func);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(handle_status);
  EXPECT_THAT(handle_status.status(),
              HasCompilerError("Column 'cpu100' not found in parent dataframe"));

  // Test for columns used in group_by arg of Agg that don't exist.
  std::string wrong_column_agg_by = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu101').agg(cpu_count=('cpu1', "
       "px.count))",
       "px.display(aggDF)"},
      "\n");
  ir_graph_status = CompileGraph(wrong_column_agg_by);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(handle_status);
  EXPECT_THAT(handle_status.status(),
              HasCompilerError("Column 'cpu101' not found in parent dataframe"));

  // Test for column not selected in From.
  std::string not_selected_col = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu2'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count))", "px.display(aggDF)"},
      "\n");
  ir_graph_status = CompileGraph(not_selected_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(handle_status);
  EXPECT_THAT(handle_status.status(),
              HasCompilerError("Column 'cpu1' not found in parent dataframe"));
}

// Use results of created columns in later parts of the pipeline.
TEST_F(AnalyzerTest, created_columns) {
  std::string agg_use_map_col_fn =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "aggDF = queryDF.groupby('cpu2').agg(", "cpu_count=('cpu_sum', px.count))",
                     "px.display(aggDF)"},
                    "\n");
  auto ir_graph_status = CompileGraph(agg_use_map_col_fn);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string agg_use_map_col_by =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "aggDF = queryDF.groupby('cpu_sum').agg(", "cpu_count=('cpu2', px.count))",
                     "px.display(aggDF)"},
                    "\n");
  ir_graph_status = CompileGraph(agg_use_map_col_by);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string map_use_agg_col = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', "
       "'cpu2'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu1').agg(cpu0_mean=('cpu0', px.mean),",
       "cpu1_mean=('cpu1', px.mean))", "aggDF['cpu_sum'] = aggDF['cpu1_mean'] + aggDF['cpu1_mean']",
       "df = aggDF[['cpu_sum']]", "px.display(df)"},
      "\n");
  ir_graph_status = CompileGraph(map_use_agg_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string map_use_map_col =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "queryDF['cpu_sum2'] = queryDF['cpu2'] + queryDF['cpu_sum']",
                     "df = queryDF[['cpu_sum2']]", "px.display(df, 'cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(map_use_map_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string agg_use_agg_col =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby('cpu1').agg(cpu0_mean=('cpu0', px.mean),",
                     "cpu1_mean=('cpu1', px.mean))", "agg2DF = aggDF.groupby('cpu1_mean').agg(",
                     "cpu0_mean_mean =('cpu0_mean', px.mean))", "px.display(aggDF)"},
                    "\n");
  ir_graph_status = CompileGraph(agg_use_agg_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
}

TEST_F(AnalyzerTest, non_float_columns) {
  std::string agg_fn_count_all = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='non_float_table', select=['float_col', 'int_col', "
          "'bool_col', "
          "'string_col'], start_time=0, end_time=10)",
          "aggDF = queryDF.groupby('float_col').agg(",
          "int_count=('int_col', px.count),",
          "bool_count=('bool_col', px.count),",
          "string_count=('string_col', px.count),",
          ")",
          "px.display(aggDF)",
      },
      "\n");
  auto ir_graph_status = CompileGraph(agg_fn_count_all);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string by_fn_count_all = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='non_float_table', select=['float_col', 'int_col', "
       "'bool_col', "
       "'string_col'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('int_col').agg(", "float_count=('float_col', px.count),",
       "bool_count=('bool_col', px.count),", "string_count=('string_col', px.count),", ")",
       "px.display(aggDF)"},
      "\n");
  ir_graph_status = CompileGraph(by_fn_count_all);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
}

TEST_F(AnalyzerTest, assign_udf_func_ids_consolidated_maps) {
  std::string chain_operators = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
       "start_time=0, end_time=10)",
       "queryDF['cpu_sub'] = queryDF['cpu0'] - queryDF['cpu1']",
       "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
       "queryDF['cpu_sum2'] = queryDF['cpu2'] + queryDF['cpu1']",
       "df = queryDF[['cpu_sum2', 'cpu_sum', 'cpu_sub']]", "px.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::vector<IRNode*> map_nodes = ir_graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 2);
  auto drop_node = static_cast<MapIR*>(map_nodes[0]);
  auto arithmetic_node = static_cast<MapIR*>(map_nodes[1]);
  // Drop node must be map with 3 expressions, otherwise we reassign.
  if (drop_node->col_exprs().size() != 3) {
    auto tmp_node = arithmetic_node;
    arithmetic_node = drop_node;
    drop_node = tmp_node;
  }

  ASSERT_THAT(*arithmetic_node->resolved_table_type(),
              IsTableType(Relation({types::FLOAT64, types::FLOAT64, types::FLOAT64, types::FLOAT64,
                                    types::FLOAT64, types::FLOAT64},
                                   {"cpu0", "cpu1", "cpu2", "cpu_sub", "cpu_sum", "cpu_sum2"})));

  ASSERT_EQ(arithmetic_node->col_exprs().size(), 6);

  auto cpu_sub_func_id = static_cast<FuncIR*>(arithmetic_node->col_exprs()[3].node)->func_id();
  auto cpu_sum_func_id = static_cast<FuncIR*>(arithmetic_node->col_exprs()[4].node)->func_id();
  auto cpu_sum2_func_id = static_cast<FuncIR*>(arithmetic_node->col_exprs()[5].node)->func_id();

  // Both should be addition functions with the same ID.
  EXPECT_EQ(cpu_sum_func_id, cpu_sum2_func_id);
  EXPECT_NE(cpu_sum_func_id, cpu_sub_func_id);
  EXPECT_THAT(std::vector<int64_t>({cpu_sum_func_id, cpu_sub_func_id}), UnorderedElementsAre(0, 1));
}

TEST_F(AnalyzerTest, assign_uda_func_ids) {
  std::string chain_operators =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby('cpu0').agg(cnt=('cpu1', px.count), mean=('cpu2', "
                     "px.mean))",
                     "px.display(aggDF)"},
                    "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  std::vector<IRNode*> agg_nodes = ir_graph->FindNodesOfType(IRNodeType::kBlockingAgg);
  EXPECT_EQ(agg_nodes.size(), 1);
  auto agg_node = static_cast<BlockingAggIR*>(agg_nodes[0]);

  auto func_node1 = static_cast<FuncIR*>(agg_node->aggregate_expressions()[0].node);
  auto func_node2 = static_cast<FuncIR*>(agg_node->aggregate_expressions()[1].node);
  EXPECT_THAT(std::vector<int64_t>({func_node1->func_id(), func_node2->func_id()}),
              UnorderedElementsAre(0, 1));
}

TEST_F(AnalyzerTest, select_all) {
  std::string select_all = "df = px.DataFrame(table='cpu')\npx.display(df, 'cpu_out')";
  auto ir_graph_status = CompileGraph("import px\n" + select_all);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  std::vector<IRNode*> sink_nodes = ir_graph->FindNodesThatMatch(ExternalGRPCSink());
  EXPECT_EQ(sink_nodes.size(), 1);
  auto sink_node = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  auto relation_map = compiler_state_->relation_map();
  ASSERT_NE(relation_map->find("cpu"), relation_map->end());
  auto expected_relation = relation_map->find("cpu")->second;
  EXPECT_THAT(*sink_node->resolved_table_type(), IsTableType(expected_relation));
}

class MetadataSingleOps : public AnalyzerTest, public ::testing::WithParamInterface<std::string> {};
TEST_P(MetadataSingleOps, valid_metadata_calls) {
  std::string op_call = GetParam();
  std::string valid_query = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table='cpu') ", "$0", "px.display(df, 'out')"}, "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  VLOG(1) << valid_query;
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}
std::vector<std::string> metadata_operators{
    "df = queryDF[queryDF.ctx['service'] == 'pl/orders']",

    "queryDF['service'] = queryDF.ctx['service']\n"
    "df = queryDF.drop(['cpu0'])",

    "queryDF['service'] = queryDF.ctx['service']\n"
    "df = queryDF.groupby('service').agg(mean_cpu=('cpu0', px.mean))",

    "queryDF['service'] = queryDF.ctx['service']\n"
    "df = queryDF.groupby(['cpu0', 'service']).agg(mean_cpu=('cpu0', px.mean))",

    "queryDF['service'] = queryDF.ctx['service']\n"
    "aggDF = queryDF.groupby(['upid', 'service']).agg(mean_cpu=('cpu0', px.mean))\n"
    "df = aggDF[aggDF.ctx['service'] == 'pl/service-name']"};

INSTANTIATE_TEST_SUITE_P(MetadataAttributesSuite, MetadataSingleOps,
                         ::testing::ValuesIn(metadata_operators));

TEST_F(AnalyzerTest, valid_metadata_call) {
  std::string valid_query =
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu') ",
                     "queryDF['service'] = queryDF.ctx['service']", "px.display(queryDF, 'out')"},
                    "\n");
  VLOG(1) << valid_query;
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}

TEST_F(AnalyzerTest, metadata_fails_no_upid) {
  std::string valid_query =
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0']) ",
                     "queryDF['service'] = queryDF.ctx['service']", "px.display(queryDF, 'out')"},
                    "\n");
  VLOG(1) << valid_query;
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  EXPECT_THAT(HandleRelation(ir_graph),
              HasCompilerError(".*Need one of \\[upid.*?. Parent type has "
                               "columns \\[cpu0\\] available."));
}

// Test to make sure that copying the metadata key column still works.
TEST_F(AnalyzerTest, copy_metadata_key_and_og_column) {
  std::string valid_query = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table='cpu')",
       "queryDF['service'] = queryDF.ctx['service']",
       "opDF = queryDF.groupby(['$0', 'service']).agg(mean_cpu =('cpu0', px.mean))",
       "opDF = opDF[opDF.ctx['service']=='pl/service-name']", "px.display(opDF, 'out')"},
      "\n");
  valid_query = absl::Substitute(valid_query, "upid");
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}

constexpr char kInnerJoinQuery[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = px.DataFrame(table='network', select=['bytes_in', 'upid', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
px.display(join, 'joined')
output = join[["upid", "bytes_in", "bytes_out", "cpu0", "cpu1"]]
px.display(output, 'mapped')
)query";

TEST_F(AnalyzerTest, join_test) {
  auto ir_graph_status = CompileGraph(kInnerJoinQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  auto join_nodes = ir_graph->FindNodesOfType(IRNodeType::kJoin);
  ASSERT_EQ(1, join_nodes.size());
  JoinIR* join = static_cast<JoinIR*>(join_nodes[0]);

  ASSERT_NE(join, nullptr);

  // check to make sure that equality conditions are properly processed.
  ASSERT_EQ(join->left_on_columns().size(), join->right_on_columns().size());
  EXPECT_EQ(join->left_on_columns()[0]->col_name(), "upid");
  EXPECT_EQ(join->right_on_columns()[0]->col_name(), "upid");

  EXPECT_EQ(join->output_columns()[0]->col_name(), "upid");
  EXPECT_EQ(join->output_columns()[1]->col_name(), "cpu0");
  EXPECT_EQ(join->output_columns()[2]->col_name(), "cpu1");
  EXPECT_EQ(join->output_columns()[3]->col_name(), "bytes_in");
  EXPECT_EQ(join->output_columns()[4]->col_name(), "upid");
  EXPECT_EQ(join->output_columns()[5]->col_name(), "bytes_out");

  EXPECT_THAT(join->column_names(),
              ElementsAre("upid", "cpu0", "cpu1", "bytes_in", "upid_x", "bytes_out"));

  ASSERT_EQ(join->Children().size(), 2);
  // Ignore the sink that is there just to preserve the join output relation.
  ASSERT_MATCH(join->Children()[0], Map());
  auto map = static_cast<MapIR*>(join->Children()[0]);

  EXPECT_THAT(
      *map->resolved_table_type(),
      IsTableType(std::vector<types::DataType>{types::UINT128, types::INT64, types::INT64,
                                               types::FLOAT64, types::FLOAT64},
                  std::vector<std::string>{"upid", "bytes_in", "bytes_out", "cpu0", "cpu1"}));
}

constexpr char kInnerJoinFollowedByMapQuery[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = px.DataFrame(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
join = join[['upid', 'bytes_in', 'bytes_out', 'cpu0', 'cpu1']]
join['mb_in'] = join['bytes_in'] / 1E6
df = join[['mb_in']]
px.display(df, 'joined')
)query";

TEST_F(AnalyzerTest, use_join_col_test) {
  auto ir_graph_status = CompileGraph(kInnerJoinFollowedByMapQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}

constexpr char kDropColumn[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0']).drop(columns=['upid'])
px.display(src1, 'dropped')
)query";

TEST_F(AnalyzerTest, drop_to_map_test) {
  auto ir_graph_status = CompileGraph(kDropColumn);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  std::vector<IRNode*> drop_nodes = ir_graph->FindNodesOfType(IRNodeType::kDrop);
  EXPECT_EQ(drop_nodes.size(), 0);

  std::vector<IRNode*> map_nodes = ir_graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 1);
  auto map = static_cast<MapIR*>(map_nodes[0]);
  table_store::schema::Relation cpu0_relation;
  cpu0_relation.AddColumn(types::FLOAT64, "cpu0");
  EXPECT_THAT(*map->resolved_table_type(), IsTableType(cpu0_relation));
}

constexpr char kDropNonexistentColumn[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0']).drop(columns=['thiscoldoesnotexist'])
px.display(src1, 'dropped')
)query";

TEST_F(AnalyzerTest, drop_to_map_nonexistent_test) {
  auto ir_graph_status = CompileGraph(kDropNonexistentColumn);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(analyzer_status);
  EXPECT_THAT(analyzer_status,
              HasCompilerError("Column 'thiscoldoesnotexist' not found in parent dataframe"));
}
constexpr char kTwoWindowQuery[] = R"query(
import px
t1 = px.DataFrame(table='http_events', start_time='-300s')
t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['resp_latency_ns'] / 1.0E6
# edit this to increase/decrease window. Dont go lower than 1 second.
t1['window1'] = px.bin(t1['time_'], px.seconds(10))
t1['window2'] = px.bin(t1['time_'] + px.seconds(5), px.seconds(10))
# groupby 1sec intervals per window
window1_agg = t1.groupby(['service', 'window1']).agg(
  quantiles=('http_resp_latency_ms', px.quantiles),
)
window1_agg['p50'] = px.pluck(window1_agg['quantiles'], 'p50')
window1_agg['p90'] = px.pluck(window1_agg['quantiles'], 'p90')
window1_agg['p99'] = px.pluck(window1_agg['quantiles'], 'p99')
window1_agg['time_'] = window1_agg['window1']
# window1_agg = window1_agg.drop('window1')

window2_agg = t1.groupby(['service', 'window2']).agg(
  quantiles=('http_resp_latency_ms', px.quantiles),
)
window2_agg['p50'] = px.pluck(window2_agg['quantiles'], 'p50')
window2_agg['p90'] = px.pluck(window2_agg['quantiles'], 'p90')
window2_agg['p99'] = px.pluck(window2_agg['quantiles'], 'p99')
window2_agg['time_'] = window2_agg['window2']
# window2_agg = window2_agg.drop('window2')

df = window2_agg[window2_agg['service'] != '']
px.display(df, 'dd')
)query";
TEST_F(AnalyzerTest, eval_compile_time_function) {
  auto ir_graph_status = CompileGraph(kTwoWindowQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_OK(analyzer_status);
}

constexpr char kCompileTimeStringFunc1[] = R"pxl(
import px
t1 = px.DataFrame(table='http_events', start_time='-5m' + '-1h')
px.display(t1)
)pxl";
constexpr char kCompileTimeStringFunc2[] = R"pxl(
import px
a = '-5m'
t1 = px.DataFrame(table='http_events', start_time=a+a)
t2 = px.DataFrame(table='http_events', start_time='-1h', end_time=a+'-15m')
px.display(t1)
px.display(t2)
)pxl";

TEST_F(AnalyzerTest, eval_compile_time_function_string_time_func) {
  auto ir_graph_status = CompileGraph(kCompileTimeStringFunc1);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_OK(analyzer_status);
}
TEST_F(AnalyzerTest, eval_compile_time_function_string_time_repeat_arg_two_ops) {
  auto ir_graph_status = CompileGraph(kCompileTimeStringFunc2);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_OK(analyzer_status);
}

constexpr char kMultiDisplays[] = R"pxl(
import px
t1 = px.DataFrame(table='http_events', start_time='-5m')
px.display(t1)
px.display(t1)
)pxl";

TEST_F(AnalyzerTest, multiple_displays_does_not_fail) {
  auto ir_graph_status = CompileGraph(kMultiDisplays);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_OK(analyzer_status);
}

TEST_F(AnalyzerTest, nested_agg_fails) {
  auto mem_src = MakeMemSource("cpu");
  auto child_fn = MakeMeanFunc(MakeColumn("cpu0", 0));
  auto parent_fn = MakeMeanFunc(child_fn);
  auto agg = MakeBlockingAgg(mem_src, {}, {{"fn", parent_fn}});
  MakeMemSink(agg, "");

  auto analyzer_status = HandleRelation(graph);
  ASSERT_NOT_OK(analyzer_status);
  EXPECT_THAT(analyzer_status.status(), HasCompilerError("agg function arg cannot be a function"));
}

constexpr char kReassignedColname[] = R"pxl(
import px
t1 = px.DataFrame(table='http_events', start_time='-5s')
t1.foo = t1.time_
t1.foo = px.bin(t1.time_, px.hours(1))
px.display(t1)
)pxl";

TEST_F(AnalyzerTest, reassigned_map_colname) {
  auto ir_graph_status = CompileGraph(kReassignedColname);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_OK(analyzer_status);
}

constexpr char kAddLimit[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0'])
px.display(src1, '1')
src1 = px.DataFrame(table='cpu', select=['upid'])
px.display(src1, '2')
)query";

TEST_F(AnalyzerTest, add_limit) {
  auto ir_graph_status = CompileGraph(kAddLimit);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  std::vector<IRNode*> src_nodes = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  EXPECT_EQ(src_nodes.size(), 2);

  for (const auto& [idx, uncast_src] : Enumerate(src_nodes)) {
    auto src = static_cast<MemorySourceIR*>(uncast_src);
    SCOPED_TRACE(absl::Substitute("src idx $0-> $1", idx, src->DebugString()));
    ASSERT_EQ(src->Children().size(), 1);
    EXPECT_MATCH(src->Children()[0], Limit(10000));
  }
}

constexpr char kAggQueryAnnotations[] = R"query(
import px
t1 = px.DataFrame(table='http_events', start_time='-5m', select=['resp_latency_ns', 'upid'])
t1.service = t1.ctx['service']
t1 = t1.groupby('service').agg(time_count=('resp_latency_ns', px.count))
t1 = t1[['service']]
px.display(t1)
)query";

TEST_F(AnalyzerTest, column_annotations_agg) {
  auto ir_graph_status = CompileGraph(kAggQueryAnnotations);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  auto aggs = ir_graph->FindNodesOfType(IRNodeType::kBlockingAgg);
  ASSERT_EQ(1, aggs.size());
  auto agg = static_cast<BlockingAggIR*>(aggs[0]);
  ASSERT_EQ(1, agg->groups().size());
  EXPECT_MATCH(agg->groups()[0], MetadataExpression(MetadataType::SERVICE_NAME));

  ASSERT_EQ(1, agg->Children().size());
  ASSERT_MATCH(agg->Children()[0], Map());
  auto map = static_cast<MapIR*>(agg->Children()[0]);
  ASSERT_EQ(1, map->col_exprs().size());
  EXPECT_MATCH(map->col_exprs()[0].node, MetadataExpression(MetadataType::SERVICE_NAME));
}

constexpr char kInnerJoinQueryAnnotations[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0'])
src1.service = src1.ctx['service']
src2 = px.DataFrame(table='network', select=['upid'])
src2.service = src2.ctx['service']
join = src1.merge(src2, how='inner', left_on=['service'], right_on=['service'], suffixes=['', '_x'])
px.display(join)
)query";

TEST_F(AnalyzerTest, column_annotations_join) {
  auto ir_graph_status = CompileGraph(kInnerJoinQueryAnnotations);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  auto joins = ir_graph->FindNodesOfType(IRNodeType::kJoin);
  ASSERT_EQ(1, joins.size());
  auto join = static_cast<JoinIR*>(joins[0]);
  ASSERT_EQ(1, join->left_on_columns().size());
  ASSERT_EQ(1, join->right_on_columns().size());
  ASSERT_EQ(5, join->output_columns().size());
  EXPECT_MATCH(join->left_on_columns()[0], MetadataExpression(MetadataType::SERVICE_NAME));
  EXPECT_MATCH(join->right_on_columns()[0], MetadataExpression(MetadataType::SERVICE_NAME));
  EXPECT_EQ(join->column_names()[2], "service");
  EXPECT_EQ(join->column_names()[4], "service_x");
  EXPECT_MATCH(join->output_columns()[2], MetadataExpression(MetadataType::SERVICE_NAME));
  EXPECT_MATCH(join->output_columns()[4], MetadataExpression(MetadataType::SERVICE_NAME));
}

constexpr char kMapOnlyStreamQuery[] = R"query(
import px
df = px.DataFrame(table='cpu', select=['upid', 'cpu0'])
df.service = df.ctx['service']
df = df[True]
px.display(df.stream())
)query";

TEST_F(AnalyzerTest, map_only_streaming_test) {
  auto ir_graph_status = CompileGraph(kMapOnlyStreamQuery);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  auto streams = ir_graph->FindNodesThatMatch(Stream());
  ASSERT_EQ(0, streams.size());

  auto limits = ir_graph->FindNodesThatMatch(Limit());
  EXPECT_EQ(0, limits.size());

  auto srcs = ir_graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(1, srcs.size());
  EXPECT_TRUE(static_cast<MemorySourceIR*>(srcs[0])->streaming());
}
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
