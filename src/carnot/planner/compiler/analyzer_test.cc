#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/parser/parser.h"

namespace pl {
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
  EXPECT_THAT(source_node->relation(),
              UnorderedRelationMatches((*compiler_state_->relation_map())["cpu"]));

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  std::vector<IRNode*> map_nodes = ir_graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 2);

  // Agg should be a new relation with one column.
  std::vector<IRNode*> agg_nodes = ir_graph->FindNodesOfType(IRNodeType::kBlockingAgg);
  EXPECT_EQ(agg_nodes.size(), 1);
  auto agg_node = static_cast<BlockingAggIR*>(agg_nodes[0]);
  table_store::schema::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  EXPECT_THAT(agg_node->relation(), UnorderedRelationMatches(test_agg_relation));

  // Sink should have the same relation as before and be equivalent to its parent.
  std::vector<IRNode*> sink_nodes = ir_graph->FindNodesOfType(IRNodeType::kMemorySink);
  EXPECT_EQ(sink_nodes.size(), 2);
  auto sink_node = static_cast<MemorySinkIR*>(sink_nodes[1]);
  EXPECT_THAT(sink_node->relation(), UnorderedRelationMatches(test_agg_relation));
  EXPECT_EQ(sink_node->relation(), sink_node->parents()[0]->relation());
}  // namespace compiler

// Make sure the compiler exits when calling columns that aren't explicitly called.
TEST_F(AnalyzerTest, test_relation_fails) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], start_time=0, "
       "end_time=10)",
       "queryDF['cpu_sum'] = queryDF['cpu0'] + queryDF['cpu1']", "queryDF = queryDF[['cpu_sum']]",
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

  // Map should result just be the cpu_sum column.
  std::vector<IRNode*> map_nodes = ir_graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 2);
  auto map_node = static_cast<MapIR*>(map_nodes[1]);
  table_store::schema::Relation test_map_relation;
  test_map_relation.AddColumn(types::FLOAT64, "cpu_sum");
  EXPECT_EQ(map_node->relation(), test_map_relation);
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
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu2");
  EXPECT_THAT(agg_node->relation(), UnorderedRelationMatches(test_agg_relation));
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
  std::vector<IRNode*> sink_nodes = ir_graph->FindNodesOfType(IRNodeType::kMemorySink);
  EXPECT_EQ(sink_nodes.size(), 1);
  auto sink_node = static_cast<MemorySinkIR*>(sink_nodes[0]);
  EXPECT_EQ(sink_node->relation(), test_relation);
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
  auto map_node = static_cast<MapIR*>(map_nodes[0]);

  EXPECT_EQ(0, static_cast<FuncIR*>(map_node->col_exprs()[3].node)->func_id());
  EXPECT_EQ(1, static_cast<FuncIR*>(map_node->col_exprs()[4].node)->func_id());
  EXPECT_EQ(1, static_cast<FuncIR*>(map_node->col_exprs()[5].node)->func_id());
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

  auto func_node = static_cast<FuncIR*>(agg_node->aggregate_expressions()[0].node);
  EXPECT_EQ(0, func_node->func_id());
  func_node = static_cast<FuncIR*>(agg_node->aggregate_expressions()[1].node);
  EXPECT_EQ(1, func_node->func_id());
}

TEST_F(AnalyzerTest, select_all) {
  std::string select_all = "df = px.DataFrame(table='cpu')\npx.display(df, 'cpu_out')";
  auto ir_graph_status = CompileGraph("import px\n" + select_all);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(HandleRelation(ir_graph));

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  std::vector<IRNode*> sink_nodes = ir_graph->FindNodesOfType(IRNodeType::kMemorySink);
  EXPECT_EQ(sink_nodes.size(), 1);
  auto sink_node = static_cast<MemorySinkIR*>(sink_nodes[0]);
  auto relation_map = compiler_state_->relation_map();
  ASSERT_NE(relation_map->find("cpu"), relation_map->end());
  auto expected_relation = relation_map->find("cpu")->second;
  EXPECT_EQ(expected_relation.col_types(), sink_node->relation().col_types());
  EXPECT_EQ(expected_relation.col_names(), sink_node->relation().col_names());
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
              HasCompilerError(".*Need one of \\[upid.*?. Parent relation has "
                               "columns \\[cpu0\\] available."));
}

TEST_F(AnalyzerTest, define_column_metadata) {
  std::string valid_query = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0']) ",
       "queryDF['$0service'] = px.add(queryDF['cpu0'], 1)", "px.display(queryDF, 'out')"},
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
      {"import px", "queryDF = px.DataFrame(table='cpu')",
       "queryDF['service'] = queryDF.ctx['service']",
       "opDF = queryDF.groupby(['$0', 'service']).agg(mean_cpu =('cpu0', px.mean))",
       "opDF = opDF[opDF.ctx['service']=='pl/service-name']", "px.display(opDF, 'out')"},
      "\n");
  valid_query = absl::Substitute(valid_query, MetadataProperty::kUniquePIDColumn);
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

  EXPECT_THAT(map->relation().col_names(),
              ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));

  EXPECT_THAT(map->relation().col_types(), ElementsAre(types::UINT128, types::INT64, types::INT64,
                                                       types::FLOAT64, types::FLOAT64));
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
  EXPECT_EQ(map->relation(), cpu0_relation);
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
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
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

// TODO(nserrino, philkuz): PL-1264 Add a  test case like start_time=a+a when that issue is fixed.
constexpr char kCompileTimeStringFunc1[] = R"pxl(
import px
t1 = px.DataFrame(table='http_events', start_time='-5m' + '-1h')
px.display(t1)
)pxl";
constexpr char kCompileTimeStringFunc2[] = R"pxl(
import px
a = '-5m'
t1 = px.DataFrame(table='http_events', start_time=a+'-10m')
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

  std::vector<IRNode*> limit_nodes = ir_graph->FindNodesOfType(IRNodeType::kLimit);
  EXPECT_EQ(limit_nodes.size(), 2);
  std::vector<IRNode*> src_nodes = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  EXPECT_EQ(src_nodes.size(), 2);

  auto limit0 = static_cast<LimitIR*>(limit_nodes[0]);
  auto limit1 = static_cast<LimitIR*>(limit_nodes[1]);
  EXPECT_THAT(limit0->parents(), ElementsAre(src_nodes[0]));
  EXPECT_THAT(limit1->parents(), ElementsAre(src_nodes[1]));
  EXPECT_EQ(10000, limit0->limit_value());
  EXPECT_EQ(10000, limit1->limit_value());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
