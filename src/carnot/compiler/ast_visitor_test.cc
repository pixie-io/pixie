#include "src/carnot/compiler/ast_visitor.h"

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

// Checks whether we can actually compile into a graph.
TEST_F(ASTVisitorTest, compilation_test) {
  std::string from_expr = "pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  auto ig_status = CompileGraph(from_expr);
  EXPECT_OK(ig_status);
  // check the connection of ig
  std::string from_range_expr =
      "pl.DataFrame(table='cpu', select=['cpu0'], start_time=0, end_time=10)";
  EXPECT_OK(CompileGraph(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST_F(ASTVisitorTest, extra_arguments) {
  std::string extra_from_args =
      "pl.DataFrame(table='cpu', select=['cpu0'], fakeArg='hahaha'start_time=0, end_time=10)";
  Status s1 = CompileGraph(extra_from_args).status();
  compilerpb::CompilerErrorGroup error_group;
  EXPECT_NOT_OK(s1);
  VLOG(1) << s1.ToString();
  // Make sure the number of context errors are as expected.
  ASSERT_TRUE(s1.has_context());
  ASSERT_TRUE(s1.context()->Is<compilerpb::CompilerErrorGroup>());
  ASSERT_TRUE(s1.context()->UnpackTo(&error_group));
  int64_t s1_num_errors = error_group.errors_size();
  ASSERT_EQ(s1_num_errors, 1);
  EXPECT_EQ(error_group.errors(0).line_col_error().line(), 1);
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 13);
  EXPECT_THAT(s1, HasCompilerError("DataFrame.* got an unexpected keyword argument 'fakeArg'"));
}

TEST_F(ASTVisitorTest, missing_one_argument) {
  std::string missing_from_args = "pl.DataFrame(select=['cpu'], start_time=0, end_time=10)";
  Status s2 = CompileGraph(missing_from_args).status();
  compilerpb::CompilerErrorGroup error_group;
  EXPECT_NOT_OK(s2);
  VLOG(1) << s2.ToString();
  // Make sure the number of context errors are as expected.
  ASSERT_TRUE(s2.has_context());
  ASSERT_TRUE(s2.context()->Is<compilerpb::CompilerErrorGroup>());
  ASSERT_TRUE(s2.context()->UnpackTo(&error_group));
  int64_t s2_num_errors = error_group.errors_size();
  ASSERT_EQ(s2_num_errors, 1);
  EXPECT_EQ(error_group.errors(0).line_col_error().line(), 1);
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 13);
  EXPECT_THAT(s2,
              HasCompilerError("DataFrame.* missing 1 required positional argument.*? 'table'"));
}

TEST_F(ASTVisitorTest, from_select_default_arg) {
  std::string no_select_arg = "df = pl.DataFrame(table='cpu')\npl.display(df)";
  EXPECT_OK(CompileGraph(no_select_arg));
}

TEST_F(ASTVisitorTest, positional_args) {
  std::string positional_arg = "df = pl.DataFrame('cpu')\npl.display(df,'out')";
  EXPECT_OK(CompileGraph(positional_arg));
}

// Checks to make sure the parser identifies bad syntax
TEST_F(ASTVisitorTest, bad_syntax) {
  std::string early_paranetheses_close = "dataframe";
  EXPECT_NOT_OK(CompileGraph(early_paranetheses_close));
}
// Checks to make sure the compiler can catch operators that don't exist.
TEST_F(ASTVisitorTest, nonexistant_operator_names) {
  std::string wrong_from_op_name =
      "notdataframe(table='cpu', select=['cpu0'], start_time=0, end_time=10)";
  auto graph_or_s = CompileGraph(wrong_from_op_name);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("name 'notdataframe' is not defined"));

  std::string wrong_range_op_name =
      "pl.DataFrame(table='cpu', select=['cpu0']).brange(start=0,stop=10)";
  graph_or_s = CompileGraph(wrong_range_op_name);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(),
              HasCompilerError("'Dataframe' object has no attribute 'brange'"));
}

TEST_F(ASTVisitorTest, assign_functionality) {
  std::string simple_assign = "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  EXPECT_OK(CompileGraph(simple_assign));
  std::string assign_and_use =
      "queryDF = pl.DataFrame('cpu', ['cpu0','cpu1'], start_time=0, end_time=10)";
  EXPECT_OK(CompileGraph(assign_and_use));
}

TEST_F(ASTVisitorTest, assign_error_checking) {
  std::string bad_assign_mult_values =
      "queryDF,haha = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  auto graph_or_s = CompileGraph(bad_assign_mult_values);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("Assignment target must be a Name"));

  std::string bad_assign_str = "queryDF = 'str'";
  graph_or_s = CompileGraph(bad_assign_str);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("Expression node 'Str' not defined"));
}

using MapTest = ASTVisitorTest;
// Map Tests
TEST_F(MapTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "rangeDF = queryDF[['sum']]",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sum));
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['div'] = pl.div(queryDF['cpu0'], queryDF['cpu1'])",
          "rangeDF = queryDF[['div']]",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_div_map_query));
}

TEST_F(MapTest, single_col_map_subscript) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['cpu2'] = queryDF['cpu0'] + queryDF['cpu1']",
      },
      "\n");
  auto result_s = CompileGraph(single_col_map_sum);
  ASSERT_OK(result_s);
  auto result = result_s.ConsumeValueOrDie();

  MapIR* map;
  for (const auto node_id : result->dag().nodes()) {
    if (result->Get(node_id)->type() == IRNodeType::kMap) {
      map = static_cast<MapIR*>(result->Get(node_id));
      break;
    }
  }
  EXPECT_NE(map, nullptr);
  EXPECT_TRUE(map->keep_input_columns());

  std::vector<std::string> output_columns;
  for (const ColumnExpression& expr : map->col_exprs()) {
    output_columns.push_back(expr.name);
  }

  EXPECT_EQ(output_columns, std::vector<std::string>{"cpu2"});
}

TEST_F(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "queryDF['copy'] = queryDF['cpu2']",
      },
      "\n");
  EXPECT_OK(CompileGraph(multi_col));
}

TEST_F(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sum));
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sub));
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['product'] = queryDF['cpu0'] * queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_product));
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['quotient'] = queryDF['cpu0'] / queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_quotient));
}

TEST_F(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1'] + queryDF['cpu2']",
      },
      "\n");
  EXPECT_OK(CompileGraph(nested_expr));
  std::string nested_fn = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['div'] = pl.div(queryDF['cpu0'] + queryDF['cpu1'], queryDF['cpu2'])",
      },
      "\n");
  EXPECT_OK(CompileGraph(nested_fn));
}

TEST_F(MapTest, wrong_df_name) {
  std::string wrong_df = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "wrong = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = wrong['cpu0'] + wrong['cpu1'] + wrong['cpu2']",
      },
      "\n");
  auto ir_graph_status = CompileGraph(wrong_df);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("name 'wrong' is not available in this context"));
}

TEST_F(MapTest, missing_df) {
  std::string wrong_df = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = dne['cpu0'] + dne['cpu1'] + dne['cpu2']",
      },
      "\n");
  auto ir_graph_status = CompileGraph(wrong_df);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("name 'dne' is not available in this context"));
}

using AggTest = ASTVisitorTest;
TEST_F(AggTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
          "'cpu1'], start_time=0, end_time=10)",
          "rangeDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count))",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_agg));
  std::string multi_output_col_agg = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "rangeDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count), cpu_mean=('cpu1', "
       "pl.mean))"},
      "\n");
  EXPECT_OK(CompileGraph(multi_output_col_agg));
  std::string multi_input_col_agg = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "rangeDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', pl.count), cpu2_mean=('cpu2', "
       "pl.mean))"},
      "\n");
  EXPECT_OK(CompileGraph(multi_input_col_agg));
}

TEST_F(AggTest, not_allowed_agg_fn) {
  std::string single_col_bad_agg_fn = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'],start_time=0, end_time=10)",
          "rangeDF = queryDF.agg(outcol=('cpu0', 1+2))",
      },
      "\n");
  auto status = CompileGraph(single_col_bad_agg_fn);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("Unexpected aggregate function"));
  std::string single_col_dict_by_not_pl = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'],start_time=0, end_time=10)",
          "rangeDF = queryDF.agg(outcol=('cpu0', notpl.sum))",
      },
      "\n");

  status = CompileGraph(single_col_dict_by_not_pl);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("name 'notpl' is not defined"));
}

using ResultTest = ASTVisitorTest;
TEST_F(ResultTest, basic) {
  std::string single_col_map_sub = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']", "df = queryDF[['sub']]",
       "pl.display(df)"},
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sub));
}

using OptionalArgs = ASTVisitorTest;
TEST_F(OptionalArgs, group_by_all) {
  std::string agg_query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "df = queryDF.agg(sum = ('cpu0', pl.sum))", "pl.display(df, 'agg')"},
                    "\n");
  EXPECT_OK(CompileGraph(agg_query));
}

TEST_F(OptionalArgs, map_copy_relation) {
  std::string map_query = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "pl.display(queryDF, 'map')"},
      "\n");
  auto graph_or_s = CompileGraph(map_query);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  auto node_or_s = FindNodeType(graph, IRNodeType::kMap);
  ASSERT_OK(node_or_s);
  MapIR* map = static_cast<MapIR*>(node_or_s.ConsumeValueOrDie());
  EXPECT_TRUE(map->keep_input_columns());
}

using RangeValueTests = ASTVisitorTest;
TEST_F(RangeValueTests, time_range_compilation) {
  // now doesn't accept args.
  std::string stop_expr = absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                                         "'cpu1'], start_time=0, end_time=pl.now()-pl.seconds(2))",
                                         "pl.display(queryDF, 'mapped')"},
                                        "\n");
  EXPECT_OK(CompileGraph(stop_expr));

  std::string start_and_stop_expr = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
       "'cpu1'], start_time=pl.now() - pl.minutes(2), end_time=pl.now()-pl.seconds(2))",
       "pl.display(queryDF, 'mapped')"},
      "\n");
  EXPECT_OK(CompileGraph(start_and_stop_expr));
}

TEST_F(RangeValueTests, implied_stop_params) {
  std::string start_expr_only =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                     "'cpu1'], start_time=pl.now() - pl.minutes(2))",
                     "pl.display(queryDF, 'mapped')"},
                    "\n");
  EXPECT_OK(CompileGraph(start_expr_only));
}

TEST_F(RangeValueTests, string_start_param) {
  std::string start_expr_only =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                     "'cpu1'], start_time='-2m')",
                     "pl.display(queryDF, 'mapped')"},
                    "\n");
  EXPECT_OK(CompileGraph(start_expr_only));
}

class FilterTestParam : public ::testing::TestWithParam<std::string> {
 protected:
  void SetUp() {
    // TODO(philkuz) use Combine with the tuple to get out a set of different values for each of the
    // values.
    compare_op_ = GetParam();
    query = absl::StrJoin(
        {"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
         "queryDF = queryDF[queryDF['cpu0'] $0 0.5]", "pl.display(queryDF, 'filtered')"},
        "\n");
    query = absl::Substitute(query, compare_op_);
    VLOG(2) << query;
  }
  std::string compare_op_;
  std::string query;
};

std::vector<std::string> comparison_functions = {">", "<", "==", ">=", "<="};

TEST_P(FilterTestParam, filter_simple_ops_test) { EXPECT_OK(ParseQuery(query)); }

INSTANTIATE_TEST_SUITE_P(FilterTestSuites, FilterTestParam,
                         ::testing::ValuesIn(comparison_functions));

using FilterExprTest = ASTVisitorTest;
TEST_F(FilterExprTest, basic) {
  // Test for and
  std::string simple_and =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] == 0.5 and queryDF['cpu1'] >= 0.2]",
                     "pl.display(queryDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(simple_and));
  // Test for or
  std::string simple_or =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] == 0.5 or queryDF['cpu1'] >= 0.2]",
                     "pl.display(queryDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(simple_or));
  // Test for nested and/or clauses
  std::string and_or_query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] == 0.5 and queryDF['cpu1'] >= 0.2 or "
                     "queryDF['cpu0'] >= 5 and queryDF['cpu1'] == 0.2]",
                     "pl.display(queryDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(and_or_query));
  // TODO(philkuz) check that and/or clauses are honored properly.
  // TODO(philkuz) handle simple math opes
}

using LimitTest = ASTVisitorTest;
TEST_F(LimitTest, basic) {
  std::string limit = absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                                     "'cpu1']).head(100)",
                                     "pl.display(queryDF, 'limited')"},
                                    "\n");
  EXPECT_OK(CompileGraph(limit));

  // No arg should work.
  std::string no_arg = absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                                      "'cpu1']).head()",
                                      "pl.display(queryDF, 'limited')"},
                                     "\n");
  EXPECT_OK(ParseQuery(no_arg));
}

TEST_F(LimitTest, limit_invalid_queries) {
  std::string string_arg = absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                                          "'cpu1']).head('arg')",
                                          "pl.display(queryDF, 'limited')"},
                                         "\n");
  // String as an arg should not work.
  EXPECT_NOT_OK(CompileGraph(string_arg));

  std::string float_arg = absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                                         "'cpu1']).head(1.2)",
                                         "pl.display(queryDF, 'limited')"},
                                        "\n");
  // float as an arg should not work.
  EXPECT_NOT_OK(CompileGraph(float_arg));
}

using NegationTest = ASTVisitorTest;
// TODO(philkuz) (PL-524) both of these changes require modifications to the actual parser.
TEST_F(NegationTest, DISABLED_bang_negation) {
  std::string bang_negation =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['bool_col']) "
                     "filterDF = queryDF[!queryDF['bool_col']]",
                     "pl.display(filterDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(bang_negation));
}

TEST_F(NegationTest, DISABLED_pythonic_negation) {
  std::string pythonic_negation =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['bool_col']) "
                     "filterDF = queryDF[not queryDF['bool_col']]",
                     "pl.display(filterDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(pythonic_negation));
}
class OpsAsAttributes : public ::testing::TestWithParam<std::string> {};
TEST_P(OpsAsAttributes, valid_attributes) {
  std::string op_call = GetParam();
  std::string invalid_query =
      absl::StrJoin({"invalid_queryDF = pl.DataFrame(table='cpu', select=['bool_col']) ",
                     "opDF = $0", "pl.display(opDF, 'out')"},
                    "\n");
  invalid_query = absl::Substitute(invalid_query, op_call);
  EXPECT_NOT_OK(ParseQuery(invalid_query));
  std::string valid_query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['bool_col']) ",
                     "opDF = queryDF.$0", "pl.display(opDF, 'out')"},
                    "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  EXPECT_OK(ParseQuery(valid_query));
}

std::vector<std::string> operators{"groupby('bool_col').agg(count=('bool_col', pl.count))",
                                   "head(n=1000)"};
INSTANTIATE_TEST_SUITE_P(OpsAsAttributesSuite, OpsAsAttributes, ::testing::ValuesIn(operators));

TEST_F(AggTest, not_allowed_by_arguments) {
  std::string single_col_bad_by_fn_expr = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "rangeDF = queryDF.groupby(1+2).agg(cpu_count=('cpu0', pl.count))",
          "pl.display(rangeDF)",
      },
      "\n");
  auto ir_graph_status = CompileGraph(single_col_bad_by_fn_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_NOT_OK(ir_graph_status);

  EXPECT_THAT(ir_graph_status.status(), HasCompilerError("expected string or list of strings"));
}

const char* kInnerJoinQuery = R"query(
src1 = pl.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = pl.DataFrame(table='network', select=['bytes_in', 'upid', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
output = join[["upid", "bytes_in", "bytes_out", "cpu0", "cpu1"]]
pl.display(output, 'joined')
)query";

using JoinTest = ASTVisitorTest;
TEST_F(JoinTest, test_inner_join) {
  auto ir_graph_status = CompileGraph(kInnerJoinQuery);
  ASSERT_OK(ir_graph_status);
  auto graph = ir_graph_status.ConsumeValueOrDie();
  MemorySourceIR* mem_src1 = nullptr;
  MemorySourceIR* mem_src2 = nullptr;
  JoinIR* join = nullptr;
  for (int64_t i : graph->dag().TopologicalSort()) {
    IRNode* node = graph->Get(i);
    if (Match(node, MemorySource())) {
      auto src = static_cast<MemorySourceIR*>(node);
      ASSERT_THAT(std::vector<std::string>({"cpu", "network"}), Contains(src->table_name()));
      if (src->table_name() == "cpu") {
        mem_src1 = src;
      } else {
        mem_src2 = src;
      }
    }

    if (Match(node, Join())) {
      join = static_cast<JoinIR*>(node);
    }
  }
  ASSERT_NE(mem_src1, nullptr);
  ASSERT_NE(mem_src2, nullptr);
  ASSERT_NE(join, nullptr);
  EXPECT_THAT(join->parents(), ElementsAre(mem_src1, mem_src2));

  EXPECT_EQ(join->left_on_columns()[0]->col_name(), "upid");
  EXPECT_EQ(join->right_on_columns()[0]->col_name(), "upid");
  EXPECT_EQ(join->left_on_columns()[0]->container_op_parent_idx(), 0);
  EXPECT_EQ(join->right_on_columns()[0]->container_op_parent_idx(), 1);

  // Output column details are set in analyzer, not in ast visitor.
  EXPECT_EQ(join->output_columns().size(), 0);
  EXPECT_EQ(join->column_names().size(), 0);

  EXPECT_EQ(join->join_type(), JoinIR::JoinType::kInner);
  EXPECT_THAT(graph->dag().ParentsOf(join->id()), ElementsAre(mem_src1->id(), mem_src2->id()));
}

const char* kJoinUnequalLeftOnRightOnColumns = R"query(
src1 = pl.DataFrame(table='cpu', select=['upid', 'cpu0'])
src2 = pl.DataFrame(table='network', select=['upid', 'bytes_in'])
join = src1.merge(src2, how='inner', left_on=['upid', 'cpu0'], right_on=['upid'])
pl.display(join, 'joined')
)query";

TEST_F(JoinTest, JoinConditionsWithUnequalLengths) {
  auto ir_graph_status = CompileGraph(kJoinUnequalLeftOnRightOnColumns);
  ASSERT_NOT_OK(ir_graph_status);
  EXPECT_THAT(
      ir_graph_status.status(),
      HasCompilerError("'left_on' and 'right_on' must contain the same number of elements."));
}

const char* kNewFilterQuery = R"query(
df = pl.DataFrame("bar")
df = df[df["service"] == "foo"]
pl.display(df, 'ld')
)query";

using FilterTest = ASTVisitorTest;
TEST_F(FilterTest, TestNewFilter) {
  auto ir_graph_or_s = CompileGraph(kNewFilterQuery);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  FilterIR* filter = nullptr;
  for (int64_t i : graph->dag().TopologicalSort()) {
    auto node = graph->Get(i);
    if (Match(node, Filter())) {
      filter = static_cast<FilterIR*>(node);
    }
  }

  ASSERT_TRUE(filter) << "Filter not found in graph.";

  ASSERT_EQ(filter->parents().size(), 1);
  ASSERT_TRUE(Match(filter->parents()[0], MemorySource()));
  auto mem_src = static_cast<MemorySourceIR*>(filter->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");

  ASSERT_TRUE(Match(filter->filter_expr(), Equals(ColumnNode(), String())));

  auto filter_expr = static_cast<FuncIR*>(filter->filter_expr());
  ASSERT_TRUE(Match(filter_expr->args()[0], ColumnNode()));
  ASSERT_TRUE(Match(filter_expr->args()[1], String()));

  ColumnIR* col = static_cast<ColumnIR*>(filter_expr->args()[0]);
  StringIR* str = static_cast<StringIR*>(filter_expr->args()[1]);
  EXPECT_EQ(col->col_name(), "service");
  EXPECT_EQ(str->str(), "foo");

  ASSERT_EQ(filter->Children().size(), 1);
  ASSERT_TRUE(Match(filter->Children()[0], MemorySink()));
}

const char* kFilterChainedQuery = R"query(
df = pl.DataFrame("bar")
df = df[df["service"] == "foo"]
pl.display(df, 'ld')
)query";

TEST_F(FilterTest, ChainedFilterQuery) {
  auto ir_graph_or_s = CompileGraph(kFilterChainedQuery);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  FilterIR* filter = nullptr;
  for (int64_t i : graph->dag().TopologicalSort()) {
    auto node = graph->Get(i);
    if (Match(node, Filter())) {
      filter = static_cast<FilterIR*>(node);
    }
  }

  ASSERT_TRUE(filter) << "Filter not found in graph.";

  ASSERT_EQ(filter->parents().size(), 1);
  ASSERT_TRUE(Match(filter->parents()[0], MemorySource()));
  auto mem_src = static_cast<MemorySourceIR*>(filter->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");

  ASSERT_TRUE(Match(filter->filter_expr(), Equals(ColumnNode(), String())));

  auto filter_expr = static_cast<FuncIR*>(filter->filter_expr());
  ASSERT_TRUE(Match(filter_expr->args()[0], ColumnNode()));
  ASSERT_TRUE(Match(filter_expr->args()[1], String()));

  ColumnIR* col = static_cast<ColumnIR*>(filter_expr->args()[0]);
  StringIR* str = static_cast<StringIR*>(filter_expr->args()[1]);
  EXPECT_EQ(col->col_name(), "service");
  EXPECT_EQ(str->str(), "foo");

  ASSERT_EQ(filter->Children().size(), 1);
  ASSERT_TRUE(Match(filter->Children()[0], MemorySink()));
}

const char* kInvalidFilterChainQuery = R"query(
df = pl.DataFrame("bar")[df["service"] == "foo"]
pl.display(df, 'ld')
)query";

// Filter can't be defined when it's chained after a node.
TEST_F(FilterTest, InvalidChainedFilterQuery) {
  auto ir_graph_or_s = CompileGraph(kInvalidFilterChainQuery);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(),
              HasCompilerError("name 'df' is not available in this context"));
}

const char* kFilterWithNewMetadataQuery = R"query(
df = pl.DataFrame("bar")
df = df[df.attr["service"] == "foo"]
pl.display(df, 'ld')
)query";

TEST_F(FilterTest, ChainedFilterWithNewMetadataQuery) {
  auto ir_graph_or_s = CompileGraph(kFilterWithNewMetadataQuery);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  FilterIR* filter = nullptr;
  for (int64_t i : graph->dag().TopologicalSort()) {
    auto node = graph->Get(i);
    if (Match(node, Filter())) {
      filter = static_cast<FilterIR*>(node);
    }
  }

  ASSERT_TRUE(filter) << "Filter not found in graph.";

  ASSERT_EQ(filter->parents().size(), 1);
  ASSERT_TRUE(Match(filter->parents()[0], MemorySource()));
  auto mem_src = static_cast<MemorySourceIR*>(filter->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");

  ASSERT_TRUE(Match(filter->filter_expr(), Equals(Metadata(), String())));

  auto filter_expr = static_cast<FuncIR*>(filter->filter_expr());
  ASSERT_TRUE(Match(filter_expr->args()[0], Metadata()));
  ASSERT_TRUE(Match(filter_expr->args()[1], String()));

  MetadataIR* col = static_cast<MetadataIR*>(filter_expr->args()[0]);
  StringIR* str = static_cast<StringIR*>(filter_expr->args()[1]);
  EXPECT_EQ(col->name(), "service");
  EXPECT_EQ(str->str(), "foo");

  ASSERT_EQ(filter->Children().size(), 1);
  ASSERT_TRUE(Match(filter->Children()[0], MemorySink()));
}

TEST_F(ASTVisitorTest, MemorySourceStartAndDefaultStop) {
  std::string query("df = pl.DataFrame('bar', start_time='-1m')\npl.display(df)");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  auto node_or_s = FindNodeType(graph, IRNodeType::kMemorySource);
  ASSERT_OK(node_or_s);
  auto node = node_or_s.ConsumeValueOrDie();

  auto mem_src = static_cast<MemorySourceIR*>(node);
  EXPECT_TRUE(mem_src->HasTimeExpressions());
  EXPECT_TRUE(Match(mem_src->start_time_expr(), String()));
  EXPECT_EQ(static_cast<StringIR*>(mem_src->start_time_expr())->str(), "-1m");
  EXPECT_TRUE(Match(mem_src->end_time_expr(), Func()));
  auto stop_time_func = static_cast<FuncIR*>(mem_src->end_time_expr());
  EXPECT_EQ(stop_time_func->func_name(), "pl.now");
}

TEST_F(ASTVisitorTest, MemorySourceDefaultStartAndStop) {
  std::string query("df = pl.DataFrame('bar')\npl.display(df)");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  auto node_or_s = FindNodeType(graph, IRNodeType::kMemorySource);
  ASSERT_OK(node_or_s);
  auto node = node_or_s.ConsumeValueOrDie();

  auto mem_src = static_cast<MemorySourceIR*>(node);
  EXPECT_FALSE(mem_src->HasTimeExpressions());
}

TEST_F(ASTVisitorTest, MemorySourceStartAndStop) {
  std::string query("df = pl.DataFrame('bar', start_time=12, end_time=100)\npl.display(df)");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  auto node_or_s = FindNodeType(graph, IRNodeType::kMemorySource);
  ASSERT_OK(node_or_s);
  auto node = node_or_s.ConsumeValueOrDie();

  auto mem_src = static_cast<MemorySourceIR*>(node);
  EXPECT_TRUE(mem_src->HasTimeExpressions());
  EXPECT_TRUE(Match(mem_src->start_time_expr(), Int()));
  EXPECT_EQ(static_cast<IntIR*>(mem_src->start_time_expr())->val(), 12);
  EXPECT_TRUE(Match(mem_src->end_time_expr(), Int()));
  EXPECT_EQ(static_cast<IntIR*>(mem_src->end_time_expr())->val(), 100);
}

TEST_F(ASTVisitorTest, DisplayTest) {
  std::string query = "df = pl.DataFrame('bar')\npl.display(df)";
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  auto node_or_s = FindNodeType(graph, IRNodeType::kMemorySink);
  ASSERT_OK(node_or_s);

  auto node = node_or_s.ConsumeValueOrDie();
  auto mem_sink = static_cast<MemorySinkIR*>(node);
  EXPECT_EQ(mem_sink->name(), "output");

  ASSERT_EQ(mem_sink->parents().size(), 1);
  ASSERT_TRUE(Match(mem_sink->parents()[0], MemorySource()));
  auto mem_src = static_cast<MemorySourceIR*>(mem_sink->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");
}

TEST_F(ASTVisitorTest, DisplayArgumentsTest) {
  std::string query("df = pl.DataFrame('bar')\npl.display(df, name='foo')");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  auto node_or_s = FindNodeType(graph, IRNodeType::kMemorySink);
  ASSERT_OK(node_or_s);

  auto node = node_or_s.ConsumeValueOrDie();
  auto mem_sink = static_cast<MemorySinkIR*>(node);
  EXPECT_EQ(mem_sink->name(), "foo");

  ASSERT_EQ(mem_sink->parents().size(), 1);
  ASSERT_TRUE(Match(mem_sink->parents()[0], MemorySource()));
  auto mem_src = static_cast<MemorySourceIR*>(mem_sink->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
