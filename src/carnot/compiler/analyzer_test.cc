#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/analyzer.h"
#include "src/carnot/compiler/parser/parser.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

using table_store::schema::Relation;
using ::testing::_;

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

class AnalyzerTest : public ASTVisitorTest {
 protected:
  Status HandleRelation(std::shared_ptr<IR> ir_graph) {
    PL_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer,
                        Analyzer::Create(compiler_state_.get()));
    return analyzer->Execute(ir_graph.get());
  }
};

TEST_F(AnalyzerTest, test_utils) {
  table_store::schema::Relation cpu2_relation;
  cpu2_relation.AddColumn(types::FLOAT64, "cpu0");
  cpu2_relation.AddColumn(types::FLOAT64, "cpu1");
  EXPECT_FALSE(RelationEquality((*compiler_state_->relation_map())["cpu"], cpu2_relation));
  EXPECT_TRUE(RelationEquality((*compiler_state_->relation_map())["cpu"],
                               (*compiler_state_->relation_map())["cpu"]));
}

TEST_F(AnalyzerTest, no_special_relation) {
  std::string from_expr =
      "df = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])\npl.display(df, 'cpu')";
  auto ir_graph_status = CompileGraph(from_expr);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  // check the connection of ig
  std::string from_range_expr =
      "df pl.DataFrame(table='cpu', select=['cpu0'], start_time=0, end_time=10)\npl.display(df)";
  ir_graph_status = CompileGraph(from_expr);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(AnalyzerTest, assign_functionality) {
  std::string assign_and_use =
      absl::StrJoin({"queryDF = pl.DataFrame(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
                     "pl.display(queryDF, 'cpu_out')"},
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
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "df = queryDF[['sum']]",
       "pl.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(single_col_map_sum);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_div_map_query = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['div'] = queryDF['cpu0'] / queryDF['cpu1']", "df = queryDF[['div']]",
       "pl.display(df, 'cpu_out')"},
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
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', "
          "'cpu2'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "queryDF['copy'] = queryDF['cpu2']",
          "df = queryDF[['sum', 'copy']]",
          "pl.display(df, 'cpu_out')",
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
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "df = queryDF[['sum']]",
       "pl.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(single_col_map_sum);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_sub = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']", "df = queryDF[['sub']]",
       "pl.display(df, 'cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(single_col_map_sub);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_product = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['product'] = queryDF['cpu0'] * queryDF['cpu1']", "df = queryDF[['product']]",
       "pl.display(df, 'cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(single_col_map_product);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_quotient = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['quotient'] = queryDF['cpu0'] / queryDF['cpu1']", "df = queryDF[['quotient']]",
       "pl.display(df, 'cpu_out')"},
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
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "df = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count))",
       "pl.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(single_col_agg);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);

  std::string multi_output_col_agg = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0','cpu1'], start_time=0, end_time=10)",
       "df = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count),",
       "cpu_mean=('cpu1', pl.mean))", "pl.display(df)"},
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
  std::string chain_operators =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['upid', 'cpu0', 'cpu1', "
                     "'cpu2', 'agent_id'], start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "groupDF = queryDF[['cpu0', 'cpu1', 'cpu_sum']].groupby('cpu0')",
                     "df = groupDF.agg(cpu_count=('cpu1', pl.count),cpu_mean=('cpu1', pl.mean))",
                     "pl.display(df)"},
                    "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  ASSERT_OK(handle_status);

  // Memory Source should copy the source relation.
  auto source_node_status = FindNodeType(ir_graph, IRNodeType::kMemorySource);
  EXPECT_OK(source_node_status);
  auto source_node = static_cast<MemorySourceIR*>(source_node_status.ConsumeValueOrDie());
  EXPECT_TRUE(RelationEquality(source_node->relation(), (*compiler_state_->relation_map())["cpu"]));
  auto mem_node_status = FindNodeType(ir_graph, IRNodeType::kMemorySink);

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  auto map_node_status = FindNodeType(ir_graph, IRNodeType::kMap, 1);
  EXPECT_OK(map_node_status);
  auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  auto test_map_relation_s =
      (*compiler_state_->relation_map())["cpu"].MakeSubRelation({"cpu0", "cpu1"});
  EXPECT_OK(test_map_relation_s);
  table_store::schema::Relation test_map_relation = test_map_relation_s.ConsumeValueOrDie();
  test_map_relation.AddColumn(types::FLOAT64, "cpu_sum");
  EXPECT_TRUE(RelationEquality(map_node->relation(), test_map_relation));

  // Agg should be a new relation with one column.
  auto agg_node_status = FindNodeType(ir_graph, IRNodeType::kBlockingAgg);
  EXPECT_OK(agg_node_status);
  auto agg_node = static_cast<BlockingAggIR*>(agg_node_status.ConsumeValueOrDie());
  table_store::schema::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  EXPECT_TRUE(RelationEquality(agg_node->relation(), test_agg_relation));

  // Sink should have the same relation as before and be equivalent to its parent.
  auto sink_node_status = FindNodeType(ir_graph, IRNodeType::kMemorySink);
  EXPECT_OK(sink_node_status);
  auto sink_node = static_cast<MemorySinkIR*>(sink_node_status.ConsumeValueOrDie());
  EXPECT_TRUE(RelationEquality(sink_node->relation(), test_agg_relation));
  EXPECT_TRUE(RelationEquality(sink_node->relation(), sink_node->parents()[0]->relation()));
}  // namespace compiler

// Make sure the compiler exits when calling columns that aren't explicitly called.
TEST_F(AnalyzerTest, test_relation_fails) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], start_time=0, "
       "end_time=10)",
       "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']", "queryDF = queryDF[['cpu_sum']]",
       "df = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count),",
       "cpu_mean=('cpu1', pl.mean))", "pl.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();

  // This query assumes implicit copying of Input relation into Map. The relation handler should
  // fail.
  auto handle_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(handle_status);

  // Map should result just be the cpu_sum column.
  auto map_node_status = FindNodeType(ir_graph, IRNodeType::kMap, 1);
  EXPECT_OK(map_node_status);
  auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  table_store::schema::Relation test_map_relation;
  test_map_relation.AddColumn(types::FLOAT64, "cpu_sum");
  EXPECT_EQ(map_node->relation(), test_map_relation);
}

TEST_F(AnalyzerTest, test_relation_multi_col_agg) {
  std::string chain_operators =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby(['cpu0', 'cpu2']).agg(cpu_count=('cpu1', pl.count),",
                     "cpu_mean=('cpu1', pl.mean))", "pl.display(aggDF)"},
                    "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  VLOG(1) << handle_status.ToString();
  ASSERT_OK(handle_status);

  auto agg_node_status = FindNodeType(ir_graph, IRNodeType::kBlockingAgg);
  EXPECT_OK(agg_node_status);
  auto agg_node = static_cast<BlockingAggIR*>(agg_node_status.ConsumeValueOrDie());
  table_store::schema::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu2");
  EXPECT_TRUE(RelationEquality(agg_node->relation(), test_agg_relation));
}

TEST_F(AnalyzerTest, test_from_select) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators =
      "queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
      "'cpu2'], start_time=0, end_time=10)\npl.display(queryDF)";
  table_store::schema::Relation test_relation;
  test_relation.AddColumn(types::FLOAT64, "cpu0");
  test_relation.AddColumn(types::FLOAT64, "cpu2");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  auto sink_node_status = FindNodeType(ir_graph, IRNodeType::kMemorySink);
  EXPECT_OK(sink_node_status);
  auto sink_node = static_cast<MemorySinkIR*>(sink_node_status.ConsumeValueOrDie());
  EXPECT_TRUE(RelationEquality(sink_node->relation(), test_relation));
}

// Test to make sure the system detects udfs/udas that don't exist.
TEST_F(AnalyzerTest, nonexistent_udfs) {
  std::string missing_udf = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['cpu_sum'] = pl.sus(queryDF['cpu0'], queryDF['cpu1'])", "df = queryDF[['cpu_sum']]",
       "pl.display(df, 'cpu_out')"},
      "\n");

  auto ir_graph_status = CompileGraph(missing_udf);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  ASSERT_NOT_OK(handle_status);
  // TODO(philkuz) convert all data udfs to be attributes of pl rather than something resolved in
  // Analyzer.
  EXPECT_THAT(handle_status.msg(), ContainsRegex("Could not find.*'pl.sus'"));
  // EXPECT_THAT(handle_status.status(), HasCompilerError("'pl' object has no attribute 'sus'"));
  std::string missing_uda = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.punt))", "pl.display(aggDF)"},
      "\n");

  ir_graph_status = CompileGraph(missing_uda);
  EXPECT_THAT(ir_graph_status.status(), HasCompilerError("'pl' object has no attribute 'punt'"));
}

TEST_F(AnalyzerTest, nonexistent_cols) {
  // Test for columns used in map function that don't exist in relation.
  std::string wrong_column_map_func = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['cpu_sum'] = pl.sum(queryDF['cpu0'], queryDF['cpu100'])",
       "df = queryDF[['cpu_sum']]", "pl.display(df, 'cpu_out')"},
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
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu101').agg(cpu_count=('cpu1', "
       "pl.count))",
       "pl.display(aggDF)"},
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
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu2'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count))", "pl.display(aggDF)"},
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
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "aggDF = queryDF.groupby('cpu2').agg(", "cpu_count=('cpu_sum', pl.count))",
                     "pl.display(aggDF)"},
                    "\n");
  auto ir_graph_status = CompileGraph(agg_use_map_col_fn);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string agg_use_map_col_by =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "aggDF = queryDF.groupby('cpu_sum').agg(", "cpu_count=('cpu2', pl.count))",
                     "pl.display(aggDF)"},
                    "\n");
  ir_graph_status = CompileGraph(agg_use_map_col_by);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string map_use_agg_col = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', "
       "'cpu2'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu1').agg(cpu0_mean=('cpu0', pl.mean),",
       "cpu1_mean=('cpu1', pl.mean))", "aggDF['cpu_sum'] = aggDF['cpu1_mean'] + aggDF['cpu1_mean']",
       "df = aggDF[['cpu_sum']]", "pl.display(df)"},
      "\n");
  ir_graph_status = CompileGraph(map_use_agg_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string map_use_map_col =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
                     "queryDF['cpu_sum2'] = queryDF['cpu2'] + queryDF['cpu_sum']",
                     "df = queryDF[['cpu_sum2']]", "pl.display(df, 'cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(map_use_map_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string agg_use_agg_col =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby('cpu1').agg(cpu0_mean=('cpu0', pl.mean),",
                     "cpu1_mean=('cpu1', pl.mean))", "agg2DF = aggDF.groupby('cpu1_mean').agg(",
                     "cpu0_mean_mean =('cpu0_mean', pl.mean))", "pl.display(aggDF)"},
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
          "queryDF = pl.DataFrame(table='non_float_table', select=['float_col', 'int_col', "
          "'bool_col', "
          "'string_col'], start_time=0, end_time=10)",
          "aggDF = queryDF.groupby('float_col').agg(",
          "int_count=('int_col', pl.count),",
          "bool_count=('bool_col', pl.count),",
          "string_count=('string_col', pl.count),",
          ")",
          "pl.display(aggDF)",
      },
      "\n");
  auto ir_graph_status = CompileGraph(agg_fn_count_all);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  std::string by_fn_count_all = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='non_float_table', select=['float_col', 'int_col', "
       "'bool_col', "
       "'string_col'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('int_col').agg(", "float_count=('float_col', pl.count),",
       "bool_count=('bool_col', pl.count),", "string_count=('string_col', pl.count),", ")",
       "pl.display(aggDF)"},
      "\n");
  ir_graph_status = CompileGraph(by_fn_count_all);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
}

TEST_F(AnalyzerTest, assign_udf_func_ids) {
  std::string chain_operators = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
       "start_time=0, end_time=10)",
       "queryDF['cpu_sub'] = queryDF['cpu0'] - queryDF['cpu1']",
       "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']",
       "queryDF['cpu_sum2'] = queryDF['cpu2'] + queryDF['cpu1']",
       "df = queryDF[['cpu_sum2', 'cpu_sum', 'cpu_sub']]", "pl.display(df, 'cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  auto map_node_status = FindNodeType(ir_graph, IRNodeType::kMap, 0);
  EXPECT_OK(map_node_status);
  auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  auto func_node = static_cast<FuncIR*>(map_node->col_exprs()[3].node);
  EXPECT_EQ(0, func_node->func_id());

  map_node_status = FindNodeType(ir_graph, IRNodeType::kMap, 1);
  EXPECT_OK(map_node_status);
  map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  func_node = static_cast<FuncIR*>(map_node->col_exprs()[4].node);
  EXPECT_EQ(1, func_node->func_id());

  map_node_status = FindNodeType(ir_graph, IRNodeType::kMap, 2);
  EXPECT_OK(map_node_status);
  map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  func_node = static_cast<FuncIR*>(map_node->col_exprs()[5].node);
  EXPECT_EQ(1, func_node->func_id());
}

TEST_F(AnalyzerTest, assign_uda_func_ids) {
  std::string chain_operators =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby('cpu0').agg(cnt=('cpu1', pl.count), mean=('cpu2', "
                     "pl.mean))",
                     "pl.display(aggDF)"},
                    "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  auto agg_node_status = FindNodeType(ir_graph, IRNodeType::kBlockingAgg);
  EXPECT_OK(agg_node_status);
  auto agg_node = static_cast<BlockingAggIR*>(agg_node_status.ConsumeValueOrDie());

  auto func_node = static_cast<FuncIR*>(agg_node->aggregate_expressions()[0].node);
  EXPECT_EQ(0, func_node->func_id());
  func_node = static_cast<FuncIR*>(agg_node->aggregate_expressions()[1].node);
  EXPECT_EQ(1, func_node->func_id());
}

TEST_F(AnalyzerTest, select_all) {
  std::string select_all = "df = pl.DataFrame(table='cpu')\npl.display(df, 'cpu_out')";
  auto ir_graph_status = CompileGraph(select_all);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  auto sink_node_status = FindNodeType(ir_graph, IRNodeType::kMemorySink);
  EXPECT_OK(sink_node_status);
  auto sink_node = static_cast<MemorySinkIR*>(sink_node_status.ConsumeValueOrDie());
  auto relation_map = compiler_state_->relation_map();
  ASSERT_NE(relation_map->find("cpu"), relation_map->end());
  auto expected_relation = relation_map->find("cpu")->second;
  EXPECT_EQ(expected_relation.col_types(), sink_node->relation().col_types());
  EXPECT_EQ(expected_relation.col_names(), sink_node->relation().col_names());
}

class MetadataSingleOps : public AnalyzerTest, public ::testing::WithParamInterface<std::string> {};
TEST_P(MetadataSingleOps, valid_metadata_calls) {
  std::string op_call = GetParam();
  std::string valid_query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu') ", "$0", "pl.display(df, 'out')"}, "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  VLOG(1) << valid_query;
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}
std::vector<std::string> metadata_operators{
    "df = queryDF[queryDF.attr['service'] == 'pl/orders']",

    "queryDF['service'] = queryDF.attr['service']\n"
    "df = queryDF.drop(['cpu0'])",

    "queryDF['service'] = queryDF.attr['service']\n"
    "df = queryDF.groupby('service').agg(mean_cpu=('cpu0', pl.mean))",

    "queryDF['service'] = queryDF.attr['service']\n"
    "df = queryDF.groupby(['cpu0', 'service']).agg(mean_cpu=('cpu0', pl.mean))",

    "queryDF['service'] = queryDF.attr['service']\n"
    "aggDF = queryDF.groupby(['upid', 'service']).agg(mean_cpu=('cpu0', pl.mean))\n"
    "df = aggDF[aggDF.attr['service'] == 'pl/service-name']"};

INSTANTIATE_TEST_SUITE_P(MetadataAttributesSuite, MetadataSingleOps,
                         ::testing::ValuesIn(metadata_operators));

TEST_F(AnalyzerTest, valid_metadata_call) {
  std::string valid_query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu') ",
                     "queryDF['service'] = queryDF.attr['service']", "pl.display(queryDF, 'out')"},
                    "\n");
  VLOG(1) << valid_query;
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}

TEST_F(AnalyzerTest, metadata_fails_no_upid) {
  std::string valid_query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0']) ",
                     "queryDF['service'] = queryDF.attr['service']", "pl.display(queryDF, 'out')"},
                    "\n");
  VLOG(1) << valid_query;
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  EXPECT_THAT(HandleRelation(ir_graph),
              HasCompilerError(".*Need one of \\[upid.*?. Parent relation has "
                               "columns \\[cpu0\\] available."));
}

TEST_F(AnalyzerTest, define_column_metadata) {
  std::string valid_query = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0']) ",
       "queryDF['$0service'] = pl.add(queryDF['cpu0'], 1)", "pl.display(queryDF, 'out')"},
      "\n");
  valid_query = absl::Substitute(valid_query, MetadataProperty::kMetadataColumnPrefix);
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  EXPECT_THAT(HandleRelation(ir_graph),
              HasCompilerError("Column name '$0service' violates naming rules. The '$0' prefix is "
                               "reserved for internal use.",
                               MetadataProperty::kMetadataColumnPrefix));
}

// Test to make sure that copying the metadata key column still works.
TEST_F(AnalyzerTest, copy_metadata_key_and_og_column) {
  std::string valid_query = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu')", "queryDF['service'] = queryDF.attr['service']",
       "opDF = queryDF.groupby(['$0', 'service']).agg(mean_cpu =('cpu0', pl.mean))",
       "opDF = opDF[opDF.attr['service']=='pl/service-name']", "pl.display(opDF, 'out')"},
      "\n");
  valid_query = absl::Substitute(valid_query, MetadataProperty::kUniquePIDColumn);
  auto ir_graph_status = CompileGraph(valid_query);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}

const char* kInnerJoinQuery = R"query(
src1 = pl.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = pl.DataFrame(table='network', select=['bytes_in', 'upid', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
output = join[["upid", "bytes_in", "bytes_out", "cpu0", "cpu1"]]
pl.display(output, 'joined')
)query";

TEST_F(AnalyzerTest, join_test) {
  auto ir_graph_status = CompileGraph(kInnerJoinQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  JoinIR* join = nullptr;
  for (int64_t i : ir_graph->dag().TopologicalSort()) {
    IRNode* node = ir_graph->Get(i);
    if (Match(node, Join())) {
      join = static_cast<JoinIR*>(node);
    }
  }
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

  ASSERT_EQ(join->Children().size(), 1);
  ASSERT_TRUE(Match(join->Children()[0], Map()));
  auto map = static_cast<MapIR*>(join->Children()[0]);

  EXPECT_THAT(map->relation().col_names(),
              ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));

  EXPECT_THAT(map->relation().col_types(), ElementsAre(types::UINT128, types::INT64, types::INT64,
                                                       types::FLOAT64, types::FLOAT64));
}

const char* kInnerJoinFollowedByMapQuery = R"query(
src1 = pl.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = pl.DataFrame(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
join = join[['upid', 'bytes_in', 'bytes_out', 'cpu0', 'cpu1']]
join['mb_in'] = join['bytes_in'] / 1E6
df = join[['mb_in']]
pl.display(df, 'joined')
)query";

TEST_F(AnalyzerTest, use_join_col_test) {
  auto ir_graph_status = CompileGraph(kInnerJoinFollowedByMapQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));
}

const char* kDropColumn = R"query(
src1 = pl.DataFrame(table='cpu', select=['upid', 'cpu0']).drop(columns=['upid'])
pl.display(src1, 'dropped')
)query";

TEST_F(AnalyzerTest, drop_to_map_test) {
  auto ir_graph_status = CompileGraph(kDropColumn);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  auto drop_node_status = FindNodeType(ir_graph, IRNodeType::kDrop);
  EXPECT_NOT_OK(drop_node_status);

  auto map_node_status = FindNodeType(ir_graph, IRNodeType::kMap);
  EXPECT_OK(map_node_status);
  auto map = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  table_store::schema::Relation cpu0_relation;
  cpu0_relation.AddColumn(types::FLOAT64, "cpu0");
  EXPECT_EQ(map->relation(), cpu0_relation);
}

const char* kDropNonexistentColumn = R"query(
src1 = pl.DataFrame(table='cpu', select=['upid', 'cpu0']).drop(columns=['thiscoldoesnotexist'])
pl.display(src1, 'dropped')
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
const char* kTwoWindowQuery = R"query(
t1 = pl.DataFrame(table='http_events', start_time='-300s')
t1['service'] = t1.attr['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
# edit this to increase/decrease window. Dont go lower than 1 second.
t1['window1'] = pl.bin(t1['time_'], pl.seconds(10))
t1['window2'] = pl.bin(t1['time_'] + pl.seconds(5), pl.seconds(10))
# groupby 1sec intervals per window
window1_agg = t1.groupby(['service', 'window1']).agg(
  quantiles=('http_resp_latency_ms', pl.quantiles),
)
window1_agg['p50'] = pl.pluck(window1_agg['quantiles'], 'p50')
window1_agg['p90'] = pl.pluck(window1_agg['quantiles'], 'p90')
window1_agg['p99'] = pl.pluck(window1_agg['quantiles'], 'p99')
window1_agg['time_'] = window1_agg['window1']
# window1_agg = window1_agg.drop('window1')

window2_agg = t1.groupby(['service', 'window2']).agg(
  quantiles=('http_resp_latency_ms', pl.quantiles),
)
window2_agg['p50'] = pl.pluck(window2_agg['quantiles'], 'p50')
window2_agg['p90'] = pl.pluck(window2_agg['quantiles'], 'p90')
window2_agg['p99'] = pl.pluck(window2_agg['quantiles'], 'p99')
window2_agg['time_'] = window2_agg['window2']
# window2_agg = window2_agg.drop('window2')

df = window2_agg[window2_agg['service'] != '']
pl.display(df, 'dd')
)query";
TEST_F(AnalyzerTest, eval_compile_time_function) {
  auto ir_graph_status = CompileGraph(kTwoWindowQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto analyzer_status = HandleRelation(ir_graph);
  ASSERT_OK(analyzer_status);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
