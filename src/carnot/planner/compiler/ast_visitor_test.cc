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

#include "src/carnot/planner/compiler/ast_visitor.h"

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/common/base/base.h"
#include "src/common/testing/status.h"
#include "src/common/testing/testing.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

// Checks whether we can actually compile into a graph.
TEST_F(ASTVisitorTest, compilation_test) {
  std::string from_expr = "import px\npx.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  auto ig_status = CompileGraph(from_expr);
  EXPECT_OK(ig_status);
  // check the connection of ig
  std::string from_range_expr =
      "import px\npx.DataFrame(table='cpu', select=['cpu0'], start_time=0, end_time=10)";
  EXPECT_OK(CompileGraph(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST_F(ASTVisitorTest, extra_arguments) {
  std::string extra_from_args =
      "import px\npx.DataFrame(table='cpu', select=['cpu0'], fakeArg='hahaha'start_time=0, "
      "end_time=10)";
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
  EXPECT_EQ(error_group.errors(0).line_col_error().line(), 2);
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 13);
  EXPECT_THAT(s1, HasCompilerError("DataFrame.* got an unexpected keyword argument 'fakeArg'"));
}

TEST_F(ASTVisitorTest, missing_one_argument) {
  std::string missing_from_args =
      "import px\npx.DataFrame(select=['cpu'], start_time=0, end_time=10)";
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
  EXPECT_EQ(error_group.errors(0).line_col_error().line(), 2);
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 13);
  EXPECT_THAT(s2,
              HasCompilerError("DataFrame.* missing 1 required positional argument.*? 'table'"));
}

TEST_F(ASTVisitorTest, from_select_default_arg) {
  std::string no_select_arg = "import px\ndf = px.DataFrame(table='cpu')\npx.display(df)";
  EXPECT_OK(CompileGraph(no_select_arg));
}

TEST_F(ASTVisitorTest, positional_args) {
  std::string positional_arg = "import px\ndf = px.DataFrame('cpu')\npx.display(df,'out')";
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
      "import px\npx.DataFrame(table='cpu', select=['cpu0']).brange(start=0,stop=10)";
  graph_or_s = CompileGraph(wrong_range_op_name);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("dataframe has no method 'brange'"));
}

TEST_F(ASTVisitorTest, assign_functionality) {
  std::string simple_assign =
      "import px\nqueryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  EXPECT_OK(CompileGraph(simple_assign));
  std::string assign_and_use =
      "import px\nqueryDF = px.DataFrame('cpu', ['cpu0','cpu1'], start_time=0, end_time=10)";
  EXPECT_OK(CompileGraph(assign_and_use));
}

TEST_F(ASTVisitorTest, assign_error_checking) {
  std::string bad_assign_mult_values =
      "import px\nqueryDF,haha = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  auto graph_or_s = CompileGraph(bad_assign_mult_values);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(),
              HasCompilerError("Assignment target must be a Name or Subscript"));
}

using MapTest = ASTVisitorTest;
// Map Tests
TEST_F(MapTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "rangeDF = queryDF[['sum']]",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sum));
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['div'] = px.divide(queryDF['cpu0'], queryDF['cpu1'])",
          "rangeDF = queryDF[['div']]",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_div_map_query));
}

TEST_F(MapTest, single_col_map_subscript_attribute) {
  std::string single_col_map = absl::StrJoin(
      {
          "import px",
          "s = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "s['cpu2'] = s['cpu0'] + s['cpu1']",

          "a = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "a['cpu2'] = a['cpu0'] + a['cpu1']",
      },
      "\n");

  auto result_s = CompileGraph(single_col_map);
  ASSERT_OK(result_s);
  auto result = result_s.ConsumeValueOrDie();

  auto mapnodes = result->FindNodesOfType(IRNodeType::kMap);
  auto map1 = static_cast<MapIR*>(mapnodes[0]);
  auto map2 = static_cast<MapIR*>(mapnodes[1]);

  EXPECT_NE(map1, nullptr);
  EXPECT_TRUE(map1->keep_input_columns());
  EXPECT_EQ(map1->keep_input_columns(), map2->keep_input_columns());

  EXPECT_EQ(map1->col_exprs().size(), map2->col_exprs().size());
  std::vector<std::string> output_columns;
  for (const auto& [idx, expr] : Enumerate(map1->col_exprs())) {
    output_columns.push_back(expr.name);
    EXPECT_EQ(expr.name, map2->col_exprs()[idx].name);
    EXPECT_TRUE(expr.node->Equals(map2->col_exprs()[idx].node));
  }
  EXPECT_EQ(output_columns, std::vector<std::string>{"cpu2"});
}

TEST_F(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "queryDF['copy'] = queryDF['cpu2']",
      },
      "\n");
  EXPECT_OK(CompileGraph(multi_col));
}

TEST_F(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sum));
  std::string single_col_map_sub = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sub));
  std::string single_col_map_product = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['product'] = queryDF['cpu0'] * queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_product));
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['quotient'] = queryDF['cpu0'] / queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_quotient));
}

TEST_F(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1'] + queryDF['cpu2']",
      },
      "\n");
  EXPECT_OK(CompileGraph(nested_expr));
  std::string nested_fn = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "queryDF['div'] = px.divide(queryDF['cpu0'] + queryDF['cpu1'], queryDF['cpu2'])",
      },
      "\n");
  EXPECT_OK(CompileGraph(nested_fn));
}

TEST_F(MapTest, wrong_df_name) {
  std::string wrong_df = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "wrong = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
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
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
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
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
          "'cpu1'], start_time=0, end_time=10)",
          "rangeDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count))",
      },
      "\n");
  EXPECT_OK(CompileGraph(single_col_agg));
  std::string multi_output_col_agg = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "rangeDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count), cpu_mean=('cpu1', "
       "px.mean))"},
      "\n");
  EXPECT_OK(CompileGraph(multi_output_col_agg));
  std::string multi_input_col_agg = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "rangeDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.count), cpu2_mean=('cpu2', "
       "px.mean))"},
      "\n");
  EXPECT_OK(CompileGraph(multi_input_col_agg));
}

TEST_F(AggTest, not_allowed_agg_fn) {
  std::string single_col_bad_agg_fn = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'],start_time=0, end_time=10)",
          "rangeDF = queryDF.agg(outcol=('cpu0', 1+2))",
      },
      "\n");
  auto status = CompileGraph(single_col_bad_agg_fn);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("Expected second tuple argument to be type Func, received Int"));
  std::string single_col_dict_by_not_pl = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'],start_time=0, end_time=10)",
          "rangeDF = queryDF.agg(outcol=('cpu0', notpx.sum))",
      },
      "\n");

  status = CompileGraph(single_col_dict_by_not_pl);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("name 'notpx' is not defined"));
}

using ResultTest = ASTVisitorTest;
TEST_F(ResultTest, basic) {
  std::string single_col_map_sub = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']", "df = queryDF[['sub']]",
       "px.display(df)"},
      "\n");
  EXPECT_OK(CompileGraph(single_col_map_sub));
}

using OptionalArgs = ASTVisitorTest;
TEST_F(OptionalArgs, group_by_all) {
  std::string agg_query =
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "df = queryDF.agg(sum = ('cpu0', px.sum))", "px.display(df, 'agg')"},
                    "\n");
  EXPECT_OK(CompileGraph(agg_query));
}

TEST_F(OptionalArgs, map_copy_relation) {
  std::string map_query = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "px.display(queryDF, 'map')"},
      "\n");
  auto graph_or_s = CompileGraph(map_query);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> map_nodes = graph->FindNodesOfType(IRNodeType::kMap);
  ASSERT_EQ(map_nodes.size(), 1);
  MapIR* map = static_cast<MapIR*>(map_nodes[0]);
  EXPECT_TRUE(map->keep_input_columns());
}

using RangeValueTests = ASTVisitorTest;
TEST_F(RangeValueTests, time_range_compilation) {
  // now doesn't accept args.
  std::string stop_expr = absl::StrJoin({"import px",
                                         "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                                         "'cpu1'], start_time=0, end_time=px.now()-px.seconds(2))",
                                         "px.display(queryDF, 'mapped')"},
                                        "\n");
  EXPECT_OK(CompileGraph(stop_expr));

  std::string start_and_stop_expr = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
       "'cpu1'], start_time=px.now() - px.minutes(2), end_time=px.now()-px.seconds(2))",
       "px.display(queryDF, 'mapped')"},
      "\n");
  EXPECT_OK(CompileGraph(start_and_stop_expr));
}

TEST_F(RangeValueTests, implied_stop_params) {
  std::string start_expr_only =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                     "'cpu1'], start_time=px.now() - px.minutes(2))",
                     "px.display(queryDF, 'mapped')"},
                    "\n");
  EXPECT_OK(CompileGraph(start_expr_only));
}

TEST_F(RangeValueTests, string_start_param) {
  std::string start_expr_only =
      absl::StrJoin({"import px",
                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                     "'cpu1'], start_time='-2m')",
                     "px.display(queryDF, 'mapped')"},
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
        {"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
         "queryDF = queryDF[queryDF['cpu0'] $0 0.5]", "px.display(queryDF, 'filtered')"},
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
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] == 0.5 and queryDF['cpu1'] >= 0.2]",
                     "px.display(queryDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(simple_and));
  // Test for or
  std::string simple_or =
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] == 0.5 or queryDF['cpu1'] >= 0.2]",
                     "px.display(queryDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(simple_or));
  // Test for nested and/or clauses
  std::string and_or_query =
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] == 0.5 and queryDF['cpu1'] >= 0.2 or "
                     "queryDF['cpu0'] >= 5 and queryDF['cpu1'] == 0.2]",
                     "px.display(queryDF, 'filtered')"},
                    "\n");
  EXPECT_OK(CompileGraph(and_or_query));
  // TODO(philkuz) check that and/or clauses are honored properly.
  // TODO(philkuz) handle simple math opes
}

using LimitTest = ASTVisitorTest;
TEST_F(LimitTest, basic) {
  std::string limit = absl::StrJoin({"import px",
                                     "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                                     "'cpu1']).head(100)",
                                     "px.display(queryDF, 'limited')"},
                                    "\n");
  EXPECT_OK(CompileGraph(limit));

  // No arg should work.
  std::string no_arg = absl::StrJoin({"import px",
                                      "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                                      "'cpu1']).head()",
                                      "px.display(queryDF, 'limited')"},
                                     "\n");
  EXPECT_OK(ParseQuery(no_arg));
}

TEST_F(LimitTest, limit_invalid_queries) {
  std::string string_arg = absl::StrJoin({"import px",
                                          "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                                          "'cpu1']).head('arg')",
                                          "px.display(queryDF, 'limited')"},
                                         "\n");
  // String as an arg should not work.
  EXPECT_NOT_OK(CompileGraph(string_arg));

  std::string float_arg = absl::StrJoin({"import px",
                                         "queryDF = px.DataFrame(table='cpu', select=['cpu0', "
                                         "'cpu1']).head(1.2)",
                                         "px.display(queryDF, 'limited')"},
                                        "\n");
  // float as an arg should not work.
  EXPECT_NOT_OK(CompileGraph(float_arg));
}

using NegationTest = ASTVisitorTest;
// TODO(philkuz) (PL-524) this change requires modifications to the actual parser.
TEST_F(NegationTest, DISABLED_bang_negation) {
  std::string bang_negation = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table='cpu', select=['bool_col']) ",
       "filterDF = queryDF[!queryDF['bool_col']]", "px.display(filterDF, 'filtered')"},
      "\n");
  EXPECT_OK(CompileGraph(bang_negation));
}

TEST_F(NegationTest, pythonic_negation) {
  std::string pythonic_negation = absl::StrJoin(
      {"import px", "queryDF = px.DataFrame(table='cpu', select=['bool_col']) ",
       "filterDF = queryDF[not queryDF['bool_col']]", "px.display(filterDF, 'filtered')"},
      "\n");
  EXPECT_OK(CompileGraph(pythonic_negation));
}
class OpsAsAttributes : public ::testing::TestWithParam<std::string> {};
TEST_P(OpsAsAttributes, valid_attributes) {
  std::string op_call = GetParam();
  std::string invalid_query = absl::StrJoin(
      {"import px", "invalid_queryDF = px.DataFrame(table='cpu', select=['bool_col']) ",
       "opDF = $0", "px.display(opDF, 'out')"},
      "\n");
  invalid_query = absl::Substitute(invalid_query, op_call);
  EXPECT_NOT_OK(ParseQuery(invalid_query));
  std::string valid_query =
      absl::StrJoin({"import px", "queryDF = px.DataFrame(table='cpu', select=['bool_col']) ",
                     "opDF = queryDF.$0", "px.display(opDF, 'out')"},
                    "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  EXPECT_OK(ParseQuery(valid_query));
}

std::vector<std::string> operators{"groupby('bool_col').agg(count=('bool_col', px.count))",
                                   "head(n=1000)"};
INSTANTIATE_TEST_SUITE_P(OpsAsAttributesSuite, OpsAsAttributes, ::testing::ValuesIn(operators));

TEST_F(AggTest, not_allowed_by_arguments) {
  std::string single_col_bad_by_fn_expr = absl::StrJoin(
      {
          "import px",
          "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "rangeDF = queryDF.groupby(1+2).agg(cpu_count=('cpu0', px.count))",
          "px.display(rangeDF)",
      },
      "\n");
  auto ir_graph_status = CompileGraph(single_col_bad_by_fn_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_NOT_OK(ir_graph_status);

  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("Expected arg 'by' as type 'String', received 'Int'"));
}

constexpr char kInnerJoinQuery[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = px.DataFrame(table='network', select=['bytes_in', 'upid', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
output = join[["upid", "bytes_in", "bytes_out", "cpu0", "cpu1"]]
px.display(output, 'joined')
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

constexpr char kJoinUnequalLeftOnRightOnColumns[] = R"query(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0'])
src2 = px.DataFrame(table='network', select=['upid', 'bytes_in'])
join = src1.merge(src2, how='inner', left_on=['upid', 'cpu0'], right_on=['upid'])
px.display(join, 'joined')
)query";

TEST_F(JoinTest, JoinConditionsWithUnequalLengths) {
  auto ir_graph_status = CompileGraph(kJoinUnequalLeftOnRightOnColumns);
  ASSERT_NOT_OK(ir_graph_status);
  EXPECT_THAT(
      ir_graph_status.status(),
      HasCompilerError("'left_on' and 'right_on' must contain the same number of elements."));
}

constexpr char kNewFilterQuery[] = R"query(
import px
df = px.DataFrame("bar")
df = df[df["service"] == "foo"]
px.display(df, 'ld')
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
  ASSERT_MATCH(filter->parents()[0], MemorySource());
  auto mem_src = static_cast<MemorySourceIR*>(filter->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");

  ASSERT_MATCH(filter->filter_expr(), Equals(ColumnNode(), String()));

  auto filter_expr = static_cast<FuncIR*>(filter->filter_expr());
  ASSERT_MATCH(filter_expr->all_args()[0], ColumnNode());
  ASSERT_MATCH(filter_expr->all_args()[1], String());

  ColumnIR* col = static_cast<ColumnIR*>(filter_expr->all_args()[0]);
  StringIR* str = static_cast<StringIR*>(filter_expr->all_args()[1]);
  EXPECT_EQ(col->col_name(), "service");
  EXPECT_EQ(str->str(), "foo");

  ASSERT_EQ(filter->Children().size(), 1);
  ASSERT_MATCH(filter->Children()[0], ExternalGRPCSink());
}

constexpr char kFilterChainedQuery[] = R"query(
import px
df = px.DataFrame("bar")
df = df[df["service"] == "foo"]
px.display(df, 'ld')
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
  ASSERT_MATCH(filter->parents()[0], MemorySource());
  auto mem_src = static_cast<MemorySourceIR*>(filter->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");

  ASSERT_MATCH(filter->filter_expr(), Equals(ColumnNode(), String()));

  auto filter_expr = static_cast<FuncIR*>(filter->filter_expr());
  ASSERT_MATCH(filter_expr->all_args()[0], ColumnNode());
  ASSERT_MATCH(filter_expr->all_args()[1], String());

  ColumnIR* col = static_cast<ColumnIR*>(filter_expr->all_args()[0]);
  StringIR* str = static_cast<StringIR*>(filter_expr->all_args()[1]);
  EXPECT_EQ(col->col_name(), "service");
  EXPECT_EQ(str->str(), "foo");

  ASSERT_EQ(filter->Children().size(), 1);
  ASSERT_MATCH(filter->Children()[0], ExternalGRPCSink());
}

constexpr char kInvalidFilterChainQuery[] = R"query(
import px
df = px.DataFrame("bar")[df["service"] == "foo"]
px.display(df, 'ld')
)query";

// Filter can't be defined when it's chained after a node.
TEST_F(FilterTest, InvalidChainedFilterQuery) {
  auto ir_graph_or_s = CompileGraph(kInvalidFilterChainQuery);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(), HasCompilerError("name 'df' is not defined"));
}

constexpr char kFilterWithNewMetadataQuery[] = R"query(
import px
df = px.DataFrame("bar")
df = df[df.ctx["service"] == "foo"]
px.display(df, 'ld')
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
  ASSERT_MATCH(filter->parents()[0], MemorySource());
  auto mem_src = static_cast<MemorySourceIR*>(filter->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");

  ASSERT_MATCH(filter->filter_expr(), Equals(Metadata(), String()));

  auto filter_expr = static_cast<FuncIR*>(filter->filter_expr());
  ASSERT_MATCH(filter_expr->all_args()[0], Metadata());
  ASSERT_MATCH(filter_expr->all_args()[1], String());

  MetadataIR* col = static_cast<MetadataIR*>(filter_expr->all_args()[0]);
  StringIR* str = static_cast<StringIR*>(filter_expr->all_args()[1]);
  EXPECT_EQ(col->name(), "service");
  EXPECT_EQ(str->str(), "foo");

  ASSERT_EQ(filter->Children().size(), 1);
  ASSERT_MATCH(filter->Children()[0], ExternalGRPCSink());
}

TEST_F(ASTVisitorTest, MemorySourceStartAndDefaultStop) {
  std::string query("import px\ndf = px.DataFrame('bar', start_time='-1m')\npx.display(df)");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);

  auto mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_TRUE(mem_src->IsTimeStartSet());
  EXPECT_FALSE(mem_src->IsTimeStopSet());
  EXPECT_EQ(
      mem_src->time_start_ns(),
      time_now -
          std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1)).count());
}

TEST_F(ASTVisitorTest, MemorySourceDefaultStartAndStop) {
  std::string query("import px\ndf = px.DataFrame('bar')\npx.display(df)");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_nodes.size(), 1);

  auto mem_src = static_cast<MemorySourceIR*>(mem_nodes[0]);
  EXPECT_FALSE(mem_src->IsTimeStartSet());
  EXPECT_FALSE(mem_src->IsTimeStopSet());
}

TEST_F(ASTVisitorTest, MemorySourceStartAndStop) {
  std::string query(
      "import px\ndf = px.DataFrame('bar', start_time=12, end_time=100)\npx.display(df)");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);

  auto mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_TRUE(mem_src->IsTimeStartSet());
  EXPECT_TRUE(mem_src->IsTimeStopSet());
  EXPECT_EQ(mem_src->time_start_ns(), 12);
  EXPECT_EQ(mem_src->time_stop_ns(), 100);
}

TEST_F(ASTVisitorTest, DisplayTest) {
  std::string query = "import px\ndf = px.DataFrame('bar')\npx.display(df)";
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> result_sinks = graph->FindNodesThatMatch(ExternalGRPCSink());

  ASSERT_EQ(result_sinks.size(), 1);

  auto result_sink = static_cast<GRPCSinkIR*>(result_sinks[0]);
  EXPECT_EQ(result_sink->name(), "output");
  EXPECT_TRUE(result_sink->DestinationAddressSet());
  EXPECT_EQ(result_sink->destination_address(), "result_addr");

  ASSERT_EQ(result_sink->parents().size(), 1);
  ASSERT_MATCH(result_sink->parents()[0], MemorySource());
  auto mem_src = static_cast<MemorySourceIR*>(result_sink->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");
}

TEST_F(ASTVisitorTest, DisplayArgumentsTest) {
  std::string query("import px\ndf = px.DataFrame('bar')\npx.display(df, name='foo')");
  auto ir_graph_or_s = CompileGraph(query);
  ASSERT_OK(ir_graph_or_s);
  auto graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> result_sinks = graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(result_sinks.size(), 1);

  auto result_sink = static_cast<GRPCSinkIR*>(result_sinks[0]);
  EXPECT_EQ(result_sink->name(), "foo");
  EXPECT_TRUE(result_sink->DestinationAddressSet());
  EXPECT_EQ(result_sink->destination_address(), "result_addr");

  ASSERT_EQ(result_sink->parents().size(), 1);
  ASSERT_MATCH(result_sink->parents()[0], MemorySource());
  auto mem_src = static_cast<MemorySourceIR*>(result_sink->parents()[0]);
  EXPECT_EQ(mem_src->table_name(), "bar");
}

// Tests whether we can evaluate operators in the argument.
TEST_F(ASTVisitorTest, DisplayWithSetupDataframe) {
  std::string query("import px\npx.display(px.DataFrame('bar'))");
  ASSERT_OK(CompileGraph(query));
}

// Tests whether we can evaluate operators in the argument.
TEST_F(ASTVisitorTest, AssignStringValueAndUseArgument) {
  std::string query("import px\na='bar'\npx.DataFrame(table=a)");
  ASSERT_OK(CompileGraph(query));
}

// Tests whether we can evaluate operators in the argument.
TEST_F(ASTVisitorTest, AssignListAndUseArgument) {
  std::string query("import px\ncolumns=['foo', 'bar', 'baz']\npx.DataFrame('cpu', columns)");
  ASSERT_OK(CompileGraph(query));
}

TEST_F(ASTVisitorTest, NonExistantUDFs) {
  std::string missing_udf = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "queryDF['cpu_sum'] = px.sus(queryDF['cpu0'], queryDF['cpu1'])", "df = queryDF[['cpu_sum']]",
       "px.display(df, 'cpu_out')"},
      "\n");

  auto ir_graph_status = CompileGraph(missing_udf);
  EXPECT_THAT(ir_graph_status.status(), HasCompilerError("object has no attribute 'sus'"));

  std::string missing_uda = absl::StrJoin(
      {"import px",
       "queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
       "aggDF = queryDF.groupby('cpu0').agg(cpu_count=('cpu1', px.punt))", "px.display(aggDF)"},
      "\n");

  ir_graph_status = CompileGraph(missing_uda);
  EXPECT_THAT(ir_graph_status.status(), HasCompilerError("object has no attribute 'punt'"));
}

TEST_F(ASTVisitorTest, CantCopyColumnsBetweenDataframes) {
  std::string query = absl::StrJoin(
      {"import px", "df1 = px.DataFrame(table='http_events').drop(['upid'])",
       "df2 = px.DataFrame(table='http_events')", "df1['upid'] = df2['upid']", "px.display(df1)"},
      "\n");
  auto ir_graph_status = CompileGraph(query);
  ASSERT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("name 'df2' is not available in this context"));
}

TEST_F(ASTVisitorTest, CantCopyMetadataBetweenDataframes) {
  std::string query = absl::StrJoin({"import px", "df1 = px.DataFrame(table='http_events')",
                                     "df2 = px.DataFrame(table='http_events')",
                                     "df1['service'] = df2.ctx['service']", "px.display(df1)"},
                                    "\n");
  auto ir_graph_status = CompileGraph(query);
  ASSERT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("name 'df2' is not available in this context"));
}

constexpr char kRepeatedExprs[] = R"query(
import px
a = 10
df = px.DataFrame("bar", start_time=a+a)
b = 20 * 20
df = df[b * b > 10]
c = px.minutes(2) / px.hours(1)
df['foo'] = c + c
px.display(df, 'ld')
)query";

TEST_F(ASTVisitorTest, test_repeated_exprs) {
  auto ir_graph_status = CompileGraph(kRepeatedExprs);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();

  // Fetch the processed args for a + a
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  EXPECT_EQ(mem_srcs.size(), 1);
  EXPECT_EQ(static_cast<MemorySourceIR*>(mem_srcs[0])->time_start_ns(), 20);

  // Fetch the processed args for b * b > 10
  std::vector<IRNode*> filters = ir_graph->FindNodesOfType(IRNodeType::kFilter);
  EXPECT_EQ(filters.size(), 1);
  auto expr2 = static_cast<FilterIR*>(filters[0])->filter_expr();
  ASSERT_MATCH(expr2, Bool(true));

  // Fetch the processed args for c + c
  std::vector<IRNode*> maps = ir_graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(maps.size(), 1);
  auto expr3 = static_cast<MapIR*>(maps[0])->col_exprs()[0].node;
  std::chrono::nanoseconds min2 = std::chrono::minutes(2);
  std::chrono::nanoseconds hr1 = std::chrono::hours(1);
  double foo_constant_value = static_cast<double>(min2.count()) / static_cast<double>(hr1.count());
  EXPECT_MATCH(expr3, Float(foo_constant_value * 2));
}

TEST_F(ASTVisitorTest, CanAccessUDTF) {
  std::string query = absl::StrJoin(
      {"import px", "df1 = px.OpenNetworkConnections('11285cdd-1de9-4ab1-ae6a-0ba08c8c676c')",
       "px.display(df1)"},
      "\n");
  auto ir_graph_status = CompileGraph(query);
  ASSERT_OK(ir_graph_status);
}

constexpr char kDefineFuncQuery[] = R"query(
import px

def func(abc):
    df = px.DataFrame(abc)
    px.display(df)

func('http_events')
)query";

TEST_F(ASTVisitorTest, define_func_query) {
  auto ir_graph_or_s = CompileGraph(kDefineFuncQuery);
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "http_events");
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], ExternalGRPCSink());
}

constexpr char kLocalStateQuery[] = R"query(
import px

a = 'foo'
def func():
    a = 'bar'

func()
# `a` should be 'foo'
df = px.DataFrame(a)
px.display(df, "out_table")
)query";

TEST_F(ASTVisitorTest, func_context_does_not_affect_global_context) {
  auto ir_graph_or_s = CompileGraph(kLocalStateQuery);
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "foo");
  ASSERT_EQ(mem_src->Children().size(), 1);
}

constexpr char kNestedFuncsIndependentState[] = R"query(
import px

a = 'foo'
def func1():
    a = 'bar'

def func2():
  func1()
  # `a` should be 'foo'
  df = px.DataFrame(a)
  px.display(df, "out_table")

func2()
)query";

TEST_F(ASTVisitorTest, nested_func_calls) {
  auto ir_graph_or_s = CompileGraph(kNestedFuncsIndependentState);
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "foo");
}

constexpr char kFuncDefWithType[] = R"query(
import px

def func(a : str):
    df = px.DataFrame(a)
    px.display(df)

$0
)query";

TEST_F(ASTVisitorTest, func_def_with_type) {
  auto ir_graph_or_s = CompileGraph(absl::Substitute(kFuncDefWithType, "func('http_events')"));
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "http_events");
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], ExternalGRPCSink());
  // Check what would happen if the wrong type is passed in.
  ir_graph_or_s = CompileGraph(absl::Substitute(kFuncDefWithType, "func(1)"));
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(), HasCompilerError("Expected 'string', received 'int64'"));
}

constexpr char kFuncDefWithDataframe[] = R"query(
import px

def func(df : px.DataFrame):
    px.display(df)

$0
)query";

TEST_F(ASTVisitorTest, func_def_with_dataframe_type) {
  auto ir_graph_or_s =
      CompileGraph(absl::Substitute(kFuncDefWithDataframe, "func(px.DataFrame('http_events'))"));
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "http_events");
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], ExternalGRPCSink());

  // Check whether non-Dataframes cause a failure.
  ir_graph_or_s = CompileGraph(absl::Substitute(kFuncDefWithDataframe, "func(1)"));
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(), HasCompilerError("Expected 'DataFrame', received 'Int'"));
}

constexpr char kFuncDefWithVarKwargs[] = R"query(
import px

def func(**kwargs):
    df = pd.DataFrame('http_events')
    px.display(df)


func('http_events')
)query";

TEST_F(ASTVisitorTest, func_def_with_kwargs_fails) {
  auto ir_graph_or_s = CompileGraph(kFuncDefWithVarKwargs);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(),
              HasCompilerError("variable length kwargs are not supported in function definitions"));
}

constexpr char kFuncDefWithVarArgs[] = R"query(
import px

def func(*args):
    df = pd.DataFrame('http_events')
    px.display(df)


func('http_events')
)query";

TEST_F(ASTVisitorTest, func_def_with_args_fails) {
  auto ir_graph_or_s = CompileGraph(kFuncDefWithVarArgs);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(),
              HasCompilerError("variable length args are not supported in function definitions"));
}

constexpr char kFuncDefWithDefaultArgs[] = R"query(
import px

def func(a = 'http_events'):
    df = px.DataFrame(a)
    px.display(df)


func('http_events')
)query";

TEST_F(ASTVisitorTest, func_def_with_default_args_fails) {
  auto ir_graph_or_s = CompileGraph(kFuncDefWithDefaultArgs);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(),
              HasCompilerError("default values not supported in function definitions"));
}

constexpr char kFuncDefWithReturn[] = R"query(
import px

def func(a):
    df = px.DataFrame(a)
    return df


df = func('http_events')
px.display(df)
)query";

TEST_F(ASTVisitorTest, func_can_return_object) {
  auto ir_graph_or_s = CompileGraph(kFuncDefWithReturn);
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "http_events");
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], ExternalGRPCSink());
}

constexpr char kRawReturnNoFuncDef[] = R"query(
import px
df = px.DataFrame('http_events')
return px.display(df)
)query";

TEST_F(ASTVisitorTest, return_outside_of_funcdef_fails) {
  // Makes sure that a return statement outside of the funcdef does not do anything.
  auto ir_graph_or_s = CompileGraph(kRawReturnNoFuncDef);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(), HasCompilerError("'return' outside function"));
}

using UnionTest = ASTVisitorTest;
// Union Tests
TEST_F(UnionTest, basic) {
  std::string union_single = absl::StrJoin(
      {
          "import px",
          "df1 = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "df2 = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=10, end_time=20)",
          "both = df1.append(df2)",
      },
      "\n");
  EXPECT_OK(CompileGraph(union_single));
  std::string union_array = absl::StrJoin(
      {
          "import px",
          "df1 = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=0, end_time=10)",
          "df2 = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=10, end_time=20)",
          "df3 = px.DataFrame(table='cpu', select=['cpu0', 'cpu1'], start_time=20, end_time=30)",
          "list_of_dfs = [df2, df3]",
          "all = df1.append(objs=list_of_dfs)",
      },
      "\n");
  EXPECT_OK(CompileGraph(union_array));
}

constexpr char kFuncDefWithEmptyReturn[] = R"query(
import px

def func(a):
    df = px.DataFrame(a)
    px.display(df)
    return


func('http_events')
)query";

TEST_F(ASTVisitorTest, func_with_empty_return) {
  // Tests to make sure we can have a function return nothing but still processes.
  auto ir_graph_or_s = CompileGraph(kFuncDefWithEmptyReturn);
  ASSERT_OK(ir_graph_or_s);
  auto ir_graph = ir_graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = ir_graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);
  EXPECT_EQ(mem_src->table_name(), "http_events");
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], ExternalGRPCSink());
}

constexpr char kFuncDefDoesntDupGlobals[] = R"pxl(
import px

int = '123'
string = 'abc'
def func():
    return int

def func2():
    return string

foo = func()
bar = func2()
)pxl";
TEST_F(ASTVisitorTest, func_def_doesnt_make_new_globals) {
  Parser parser;
  auto ast_or_s = parser.Parse(kFuncDefDoesntDupGlobals);
  ASSERT_OK(ast_or_s);
  auto ast = ast_or_s.ConsumeValueOrDie();
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ModuleHandler module_handler;
  MutationsIR dynamic_trace;
  auto ast_walker_or_s =
      ASTVisitorImpl::Create(ir.get(), &dynamic_trace, compiler_state_.get(), &module_handler);
  ASSERT_OK(ast_walker_or_s);
  auto ast_walker = ast_walker_or_s.ConsumeValueOrDie();
  ASSERT_OK(ast_walker->ProcessModuleNode(ast));
  auto var_table = ast_walker->var_table();

  // If the ast_visitor recreates `int` in the new function
  // then the return value of the function will be the Int TypeObject
  // and thus won't have a node
  auto func_return_obj = var_table->Lookup("foo");
  ASSERT_OK_AND_ASSIGN(auto return1, GetAsString(func_return_obj));
  ASSERT_EQ(return1, "123");

  auto func2_return_obj = var_table->Lookup("string");
  ASSERT_OK_AND_ASSIGN(auto return2, GetAsString(func2_return_obj));
  ASSERT_EQ(return2, "abc");
}

constexpr char kReassignPixieMethodsQuery[] = R"pxl(
import px
pixie = px
make_a_df = pixie.DataFrame
px.my_new_attr = 'cpu0'
queryDF = make_a_df(table='cpu', select=[px.my_new_attr])
pixie.display(queryDF, 'table_name')
)pxl";

TEST_F(ASTVisitorTest, reassign_px_attrs) {
  auto graph_or_s = CompileGraph(kReassignPixieMethodsQuery);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> src_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  std::vector<IRNode*> sink_nodes = graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(src_nodes.size(), 1);
  ASSERT_EQ(sink_nodes.size(), 1);
  MemorySourceIR* src = static_cast<MemorySourceIR*>(src_nodes[0]);
  EXPECT_THAT(src->column_names(), ElementsAre("cpu0"));
  GRPCSinkIR* sink = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  EXPECT_EQ("table_name", sink->name());
  EXPECT_THAT(sink->parents(), ElementsAre(src));
}

constexpr char kImportAsQuery[] = R"query(
import px as pixie
df = pixie.DataFrame("bar")
df = df[df["service"] == "foo"]
pixie.display(df, 'ld')
)query";

TEST_F(ASTVisitorTest, import_as) {
  auto graph_or_s = CompileGraph(kImportAsQuery);
  ASSERT_OK(graph_or_s);
}

constexpr char kImportFromQuery[] = R"query(
from px import DataFrame
import px
df = DataFrame("bar")
df = df[df["service"] == "foo"]
px.display(df, 'ld')
)query";

TEST_F(ASTVisitorTest, from_import) {
  auto graph_or_s = CompileGraph(kImportFromQuery);
  ASSERT_OK(graph_or_s);
}

constexpr char kImportFromQueryWithAlias[] = R"query(
from px import DataFrame as DFrame, display
df = DFrame("bar")
df = df[df["service"] == "foo"]
display(df, 'ld')
)query";

TEST_F(ASTVisitorTest, from_import_with_alias) {
  auto graph_or_s = CompileGraph(kImportFromQueryWithAlias);
  ASSERT_OK(graph_or_s);
}

constexpr char kDecoratorParsing[] = R"pxl(
import px
@px.vis.vega("""
abcd
""")
def plot_latency():
    return pd.DataFrame('http_events')
)pxl";

TEST_F(ASTVisitorTest, decorator_parsed) {
  Parser parser;
  pypa::AstModulePtr ast = parser.Parse(kDecoratorParsing).ConsumeValueOrDie();
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ModuleHandler module_handler;
  MutationsIR dynamic_trace;
  auto ast_walker = compiler::ASTVisitorImpl::Create(ir.get(), &dynamic_trace,
                                                     compiler_state_.get(), &module_handler)
                        .ConsumeValueOrDie();

  ASSERT_OK(ast_walker->ProcessModuleNode(ast));

  ASSERT_TRUE(ast_walker->var_table()->HasVariable("plot_latency"));
  auto plot_latency = ast_walker->var_table()->Lookup("plot_latency");
  ASSERT_TRUE(plot_latency->type() == QLObjectType::kFunction);

  auto func_object = std::static_pointer_cast<FuncObject>(plot_latency);
  ASSERT_TRUE(func_object->HasVisSpec());
  EXPECT_EQ(func_object->vis_spec()->vega_spec, "\nabcd\n");
}

constexpr char kProblemDecoratorParsing[] = R"pxl(
import px
@px.vis
def plot_latency():
    return pd.DataFrame('http_events')
)pxl";

TEST_F(ASTVisitorTest, problem_decorator_parsed) {
  EXPECT_THAT(CompileGraph(kProblemDecoratorParsing).status(),
              HasCompilerError("'vis' object is not callable"));
}

constexpr char kGlobalDocStringQuery[] = R"pxl(
"""This is a global doc string."""
import px
df = px.DataFrame(table='cpu', select=['cpu0'])
px.display(df, __doc__)
)pxl";

TEST_F(ASTVisitorTest, global_doc_string) {
  auto graph_or_s = CompileGraph(kGlobalDocStringQuery);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> sink_nodes = graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(sink_nodes.size(), 1);
  GRPCSinkIR* sink = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  EXPECT_EQ("This is a global doc string.", sink->name());
}

constexpr char kFuncDocStringQuery[] = R"pxl(
import px
def f():
  """This is a function doc string."""
  return px.DataFrame(table='cpu', select=['cpu0'])
px.display(f())
)pxl";

TEST_F(ASTVisitorTest, func_doc_string) {
  // Make sure that a function documentation string still allows a function to be called.
  auto graph_or_s = CompileGraph(kFuncDocStringQuery);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  std::vector<IRNode*> source_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(source_nodes.size(), 1);
  MemorySourceIR* src = static_cast<MemorySourceIR*>(source_nodes[0]);
  EXPECT_EQ("cpu", src->table_name());
}

constexpr char kArgAnnotationsQuery[] = R"pxl(
import px
@px.vis.vega("vega")
def f(a: int, b: str, c: float, d: bool, e: px.Time):
  return 1
)pxl";

TEST_F(ASTVisitorTest, arg_annotations) {
  auto ast_walker_or_s = CompileInspectAST(kArgAnnotationsQuery);
  ASSERT_OK(ast_walker_or_s);
  auto ast_walker = ast_walker_or_s.ConsumeValueOrDie();
  auto qlobjptr = ast_walker->var_table()->Lookup("f");
  ASSERT_EQ(qlobjptr->type(), QLObjectType::kFunction);
  auto func_obj = std::static_pointer_cast<FuncObject>(qlobjptr);
  auto arg_type_objs = func_obj->arg_types();
  absl::flat_hash_map<std::string, px::types::DataType> arg_types;
  for (const auto& [name, type_obj] : arg_type_objs) {
    arg_types[name] = type_obj->data_type();
  }
  absl::flat_hash_map<std::string, px::types::DataType> expected_types({
      {"a", px::types::DataType::INT64},
      {"b", px::types::DataType::STRING},
      {"c", px::types::DataType::FLOAT64},
      {"d", px::types::DataType::BOOLEAN},
      {"e", px::types::DataType::TIME64NS},
  });
  ASSERT_EQ(arg_types, expected_types);
}

TEST_F(ASTVisitorTest, true_false_test) {
  std::string bool_use_and_reuse_test =
      absl::StrJoin({"import px", "df = px.DataFrame('cpu')", "df = df[False]", "df = df[True]",
                     "df = df[False]", "px.display(df, 'mapped')"},
                    "\n");
  EXPECT_OK(CompileGraph(bool_use_and_reuse_test));
}

constexpr char kExecFuncsQuery[] = R"pxl(
import px
def f(start_time: px.Time, name: str):
  df = px.DataFrame('http_events', start_time=start_time)
  df[name] = df.resp_latency_ns
  return df

)pxl";

TEST_F(ASTVisitorTest, compile_with_exec_funcs) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("test");
  auto start_time = f.add_arg_values();
  start_time->set_name("start_time");
  start_time->set_value("1234");
  auto name = f.add_arg_values();
  name->set_name("name");
  name->set_value("my column name");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsQuery, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> source_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(source_nodes.size(), 1);
  MemorySourceIR* source = static_cast<MemorySourceIR*>(source_nodes[0]);
  ASSERT_EQ(source->time_start_ns(), 1234);

  std::vector<IRNode*> map_nodes = graph->FindNodesOfType(IRNodeType::kMap);
  ASSERT_EQ(map_nodes.size(), 1);
  MapIR* map = static_cast<MapIR*>(map_nodes[0]);
  ASSERT_EQ(map->col_exprs().size(), 1);
  EXPECT_EQ(map->col_exprs()[0].name, "my column name");

  std::vector<IRNode*> sink_nodes = graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(sink_nodes.size(), 1);
  GRPCSinkIR* sink = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  EXPECT_EQ(sink->name(), "test");
}

constexpr char kExecFuncsWithDisplayQuery[] = R"pxl(
import px
def f():
  return px.DataFrame('http_events')
# this px.display should be ignored, but the source will still be created.
px.display(px.DataFrame('http_events'))
)pxl";

TEST_F(ASTVisitorTest, compile_with_exec_funcs_and_display) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("test");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsWithDisplayQuery, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> source_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(source_nodes.size(), 2);

  std::vector<IRNode*> sink_nodes = graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(sink_nodes.size(), 1);
  GRPCSinkIR* sink = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  EXPECT_EQ(sink->name(), "test");
}

constexpr char kExecFuncsWithDebugQuery[] = R"pxl(
import px
def f():
  return px.DataFrame('http_events')
# px debug should create a new sink with the debug prefix.
px.debug(px.DataFrame('http_events'))
)pxl";

TEST_F(ASTVisitorTest, compile_with_exec_funcs_and_debug) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("test");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsWithDebugQuery, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> source_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(source_nodes.size(), 2);

  std::vector<std::string> sink_names;
  for (auto node : graph->FindNodesThatMatch(ExternalGRPCSink())) {
    GRPCSinkIR* sink = static_cast<GRPCSinkIR*>(node);
    sink_names.push_back(sink->name());
  }
  EXPECT_THAT(sink_names, UnorderedElementsAre("_output", "test"));
}

constexpr char kExecFuncsWithDuplicateDebugQuery[] = R"pxl(
import px
def f():
  return px.DataFrame('http_events')
# This px.debug should create a sink w/ a de-duplicated name because the exec function has the same name after prefixing.
px.debug(px.DataFrame('http_events'), 'test')
)pxl";

TEST_F(ASTVisitorTest, compile_with_exec_funcs_and_duplicate_display_name) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("_test");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsWithDuplicateDebugQuery, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> source_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(source_nodes.size(), 2);

  std::vector<std::string> sink_names;
  for (auto node : graph->FindNodesThatMatch(ExternalGRPCSink())) {
    GRPCSinkIR* sink = static_cast<GRPCSinkIR*>(node);
    sink_names.push_back(sink->name());
  }
  EXPECT_THAT(sink_names, UnorderedElementsAre("_test_1", "_test"));
}

constexpr char kExecFuncsWithGlobals[] = R"pxl(
import px
df = px.DataFrame('http_events', select=['resp_latency_ns'])
def f():
  df.resp_latency_ns = 2 * df.resp_latency_ns
  return df

def g():
  df.resp_latency_ns = 3 * df.resp_latency_ns
  return df
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_with_globals) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("f_out");
  FuncToExecute g;
  g.set_func_name("g");
  g.set_output_table_prefix("g_out");
  ExecFuncs exec_funcs({f, g});

  auto graph_or_s = CompileGraph(kExecFuncsWithGlobals, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> source_nodes = graph->FindNodesOfType(IRNodeType::kMemorySource);
  // f and g should use the same source.
  ASSERT_EQ(source_nodes.size(), 1);

  std::vector<IRNode*> map_nodes = graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 2);
  for (const auto& node : map_nodes) {
    auto map = static_cast<MapIR*>(node);
    // Maps for f and g, should have the source node as the parent.
    // This tests that the global state is not shared between the function calls.
    EXPECT_EQ(map->parents().size(), 1);
    EXPECT_TRUE(map->IsChildOf(static_cast<OperatorIR*>(source_nodes[0])));
  }

  std::vector<IRNode*> sink_nodes = graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(sink_nodes.size(), 2);
}

constexpr char kExecFuncsArgParsingQuery[] = R"pxl(
import px
df = px.DataFrame('http_events')

def f(a: int, b:str, c: float, d: bool, e: bool):
  df.a = d
  df.b = e
  return df
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_arg_parsing) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("f_out");
  auto a = f.add_arg_values();
  a->set_name("a");
  a->set_value("1234");
  auto b = f.add_arg_values();
  b->set_name("b");
  b->set_value("test string");
  auto c = f.add_arg_values();
  c->set_name("c");
  c->set_value("1.234");
  auto d = f.add_arg_values();
  d->set_name("d");
  d->set_value("true");
  auto e = f.add_arg_values();
  e->set_name("e");
  e->set_value("True");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsArgParsingQuery, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> str_nodes = graph->FindNodesOfType(IRNodeType::kString);
  EXPECT_GE(str_nodes.size(), 2);
  std::vector<std::string> str_vals;
  for (const auto& node : str_nodes) {
    auto str = static_cast<StringIR*>(node);
    str_vals.push_back(str->str());
  }
  EXPECT_NE(std::find(str_vals.begin(), str_vals.end(), "test string"), str_vals.end());

  std::vector<IRNode*> int_nodes = graph->FindNodesOfType(IRNodeType::kInt);
  EXPECT_GE(int_nodes.size(), 1);
  std::vector<int64_t> int_vals;
  for (const auto& node : int_nodes) {
    int_vals.push_back(static_cast<IntIR*>(node)->val());
  }
  EXPECT_NE(std::find(int_vals.begin(), int_vals.end(), 1234), int_vals.end());

  std::vector<IRNode*> float_nodes = graph->FindNodesOfType(IRNodeType::kFloat);
  EXPECT_GE(float_nodes.size(), 1);
  std::vector<double> float_vals;
  for (const auto& node : float_nodes) {
    float_vals.push_back(static_cast<FloatIR*>(node)->val());
  }
  EXPECT_NE(std::find(float_vals.begin(), float_vals.end(), 1.234), float_vals.end());

  std::vector<IRNode*> bool_nodes = graph->FindNodesOfType(IRNodeType::kBool);
  EXPECT_GE(bool_nodes.size(), 2);
  for (const auto& node : bool_nodes) {
    auto parents = graph->dag().ParentsOf(node->id());
    for (auto parent : parents) {
      if (graph->Get(parent)->type() == IRNodeType::kMap) {
        EXPECT_EQ(static_cast<BoolIR*>(node)->val(), true);
      }
    }
  }
}

constexpr char kExecFuncsMultipleReturnsQuery[] = R"pxl(
import px
def f():
  df1 = px.DataFrame('http_events')
  df2 = px.DataFrame('http_events')
  return df1, df2

def g():
  df1 = px.DataFrame('http_events')
  df2 = px.DataFrame('http_events')
  return [df1, df2]
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_multiple_returns) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("f_out");
  FuncToExecute g;
  g.set_func_name("g");
  g.set_output_table_prefix("g_out");
  ExecFuncs exec_funcs({f, g});

  auto graph_or_s = CompileGraph(kExecFuncsMultipleReturnsQuery, exec_funcs);
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();

  std::vector<IRNode*> sink_nodes = graph->FindNodesThatMatch(ExternalGRPCSink());
  EXPECT_EQ(sink_nodes.size(), 4);
  std::vector<std::string> sink_names;
  for (const auto& node : sink_nodes) {
    sink_names.push_back(static_cast<GRPCSinkIR*>(node)->name());
  }
  EXPECT_THAT(sink_names, UnorderedElementsAre("f_out[0]", "f_out[1]", "g_out[0]", "g_out[1]"));
}

constexpr char kExecFuncsReturnNotDf[] = R"pxl(
import px
def f():
  return "abcd"
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_return_not_df) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("f_out");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsReturnNotDf, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(),
              HasCompilerError("Function 'f' returns 'String' but should return a DataFrame."));
}

constexpr char kExecFuncsMultipleReturnNotDf[] = R"pxl(
import px
def f():
  return [px.DataFrame('http_events'), "abcd"]
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_multiple_returns_not_df) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("f_out");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsMultipleReturnNotDf, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(
      graph_or_s.status(),
      HasCompilerError(
          "Function 'f' returns 'String' at index 1. All returned objects must be dataframes."));
}

constexpr char kExecFuncsMustSpecifyOutputPrefix[] = R"pxl(
import px
def f():
  return px.DataFrame('http_events')
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_must_specify_output_prefix) {
  FuncToExecute f;
  f.set_func_name("f");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsMustSpecifyOutputPrefix, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(),
              HasCompilerError("Output_table_prefix must be specified for function f."));
}

TEST_F(ASTVisitorTest, exec_funcs_must_specify_func_name) {
  FuncToExecute f;
  f.set_output_table_prefix("f_out");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsMustSpecifyOutputPrefix, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(),
              HasCompilerError("Must specify func_name for each FuncToExecute."));
}

TEST_F(ASTVisitorTest, exec_funcs_missing_func_name) {
  FuncToExecute g;
  g.set_func_name("g");
  g.set_output_table_prefix("g_out");
  ExecFuncs exec_funcs({g});

  auto graph_or_s = CompileGraph(kExecFuncsMustSpecifyOutputPrefix, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("Function to execute, 'g', not found."));
}

constexpr char kExecFuncsFuncNotFunc[] = R"pxl(
import px
f = px.DataFrame('http_events')
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_func_not_a_function) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("f_out");
  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kExecFuncsFuncNotFunc, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("'f' is a 'DataFrame' not a function."));
}

constexpr char kPlanExecFuncsListArg[] = R"pxl(
import px
def f(cols: list):
  return px.DataFrame('http_events', select=cols, start_time='-2m')
)pxl";

TEST_F(ASTVisitorTest, exec_funcs_list_arg) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("test");
  auto a = f.add_arg_values();
  a->set_name("cols");
  a->set_value("['resp_status', 'upid', 'time_']");

  ExecFuncs exec_funcs({f});

  ASSERT_OK_AND_ASSIGN(auto graph, CompileGraph(kPlanExecFuncsListArg, exec_funcs));
  auto mem_srcs = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(mem_srcs.size(), 1);

  EXPECT_THAT(static_cast<MemorySourceIR*>(mem_srcs[0])->column_names(),
              ElementsAre("resp_status", "upid", "time_"));
}

TEST_F(ASTVisitorTest, exec_funcs_list_arg_syntax_error) {
  FuncToExecute f;
  f.set_func_name("f");
  f.set_output_table_prefix("test");
  auto a = f.add_arg_values();
  a->set_name("cols");

  // Missing opening bracket here.
  a->set_value("'resp_status', 'upid', 'time_']");

  ExecFuncs exec_funcs({f});

  auto graph_or_s = CompileGraph(kPlanExecFuncsListArg, exec_funcs);
  ASSERT_NOT_OK(graph_or_s);
  EXPECT_THAT(graph_or_s.status(), HasCompilerError("Failed to parse arg"));
}

TEST_F(ASTVisitorTest, unary_ops_execute_underlying_udf_funcs) {
  std::string keep_duplicates_expr = R"pxl(import px
df = px.DataFrame('http_events')
df.test = -(10 * -2)
px.display(df)
)pxl";

  ASSERT_OK_AND_ASSIGN(auto graph, CompileGraph(keep_duplicates_expr, {}));
  auto int_node = graph->FindNodesThatMatch(Int(20));
  ASSERT_EQ(int_node.size(), 1);
}

// The function definition Module.
constexpr char kFuncsUtilsModule[] = R"pxl(
import px
def funcs():
    '''
    Merge the func helpers together
    '''
    return px.DataFrame('http_events')

free_var = "imfree"
)pxl";

constexpr char kFuncsUtilsTest[] = R"pxl(
import funcs_utils
import px
px.display(funcs_utils.funcs())
)pxl";

TEST_F(ASTVisitorTest, alt_imports_test_normal) {
  PX_UNUSED(kFuncsUtilsModule);
  auto graph_or_s = CompileGraph(kFuncsUtilsTest, {}, {{"funcs_utils", kFuncsUtilsModule}});
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  auto sinks = graph->FindNodesThatMatch(ExternalGRPCSink());
  auto sink = static_cast<GRPCSinkIR*>(sinks[0]);
  EXPECT_MATCH(sink->parents()[0], MemorySource());
}

constexpr char kFromFuncsUtilsTest[] = R"pxl(
import px
from funcs_utils import funcs
px.display(funcs())
)pxl";

TEST_F(ASTVisitorTest, alt_imports_test_from) {
  auto graph_or_s = CompileGraph(kFromFuncsUtilsTest, {}, {{"funcs_utils", kFuncsUtilsModule}});
  ASSERT_OK(graph_or_s);
  auto graph = graph_or_s.ConsumeValueOrDie();
  auto sinks = graph->FindNodesThatMatch(ExternalGRPCSink());
  auto sink = static_cast<GRPCSinkIR*>(sinks[0]);
  EXPECT_MATCH(sink->parents()[0], MemorySource());
}

constexpr char kReuseLimitArg[] = R"pxl(
import px

start_time = '-3m'
max_num_records = 500
def embeddings():
  df = px.DataFrame(table='http_events', start_time='-3m')
  return df.head(max_num_records)
px.display(embeddings())
px.display(embeddings())
)pxl";
TEST_F(ASTVisitorTest, reuse_limit_arg) { ASSERT_OK(CompileGraph(kReuseLimitArg, {}, {})); }

constexpr char kAssignToGroupby[] = R"pxl(
import px

def resource_timeseries(start_time: str, node: px.Node, groupby: str):
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.groupby = df[groupby]
    return df
px.display(resource_timeseries('-5m', '', 'pod'))
)pxl";
TEST_F(ASTVisitorTest, rsrc_ts) {
  EXPECT_COMPILER_ERROR(CompileGraph(kAssignToGroupby, {}, {}),
                        "Expected 'Column' in arg 'assignment target', got 'function'");
}
constexpr char kCompileTimeStringConcat[] = R"pxl(
import px
px.display(px.DataFrame(table='http' + '_events'))
)pxl";
TEST_F(ASTVisitorTest, compile_time_string_concat) {
  ASSERT_OK(CompileGraph(kCompileTimeStringConcat, {}, {}));
}

// Previously compile time only worked outside of function defs. This makes sure it works in
// function defs.
constexpr char kCompileTimeInFunction[] = R"pxl(
import px
def fn():
  return px.DataFrame(table='http' + '_events')
px.display(fn())
)pxl";
TEST_F(ASTVisitorTest, compile_time_string_concat_in_function) {
  ASSERT_OK_AND_ASSIGN(auto graph, CompileGraph(kCompileTimeInFunction, {}, {}));
  auto mem_src = static_cast<MemorySourceIR*>(graph->FindNodesThatMatch(MemorySource())[0]);
  EXPECT_EQ(mem_src->table_name(), "http_events");
}

TEST_F(ASTVisitorTest, keep_duplicate_test) {
  std::string keep_duplicates_expr = R"pxl(import px
queryDF = px.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'upid'])
queryDF = queryDF['upid', 'cpu1', 'upid', 'cpu0']
)pxl";

  EXPECT_COMPILER_ERROR_AT(
      CompileGraph(keep_duplicates_expr), 3, 35,
      "cannot specify the same column name more than once when filtering by cols. 'upid' "
      "specified more than once");
}

constexpr char kAggSegfaultScript[] = R"pxl(
import px
# ----------------------------------------------------------------
# Script variables
# ----------------------------------------------------------------
start_time = '-30s'
max_num_records = 100
# ----------------------------------------------------------------
# Implementation
# ----------------------------------------------------------------
df = px.DataFrame(table='http_events', start_time=start_time)
df.pod = df.ctx['pod']
df = df.drop(['upid', 'trace_role', 'minor_version', 'content_type', 'resp_message'])
df = df.groupby(['pod']).agg(count=(px.count, 'upid'))
df = df.head(n=max_num_records)
px.display(df)
)pxl";
TEST_F(ASTVisitorTest, agg_segfault) {
  EXPECT_THAT(CompileGraph(kAggSegfaultScript).status(),
              HasCompilerError(".*?All elements of the agg tuple must be column names.*?"));
}

TEST_F(ASTVisitorTest, error_on_global) {
  // Tests to make sure that we will have a valid error on global objects that don't start with ast
  // errors. AstVisitorImpl::LookupVariable should set the ast for any referenced object.
  auto vt = VarTable::Create();
  EXPECT_OK(ParseScript(vt, "l = list"));
  EXPECT_THAT(vt->Lookup("l")->CreateError("new error"), HasCompilerErrorAt(1, 5, "new error"));
}

TEST_F(ASTVisitorTest, module_uses_global_variable) {
  std::string moduleUsesGlobal = R"pxl(
def return_bool():
  return True
)pxl";
  std::string query = R"pxl(
import tmp
import px
df = px.DataFrame('http_events')
df.column = tmp.return_bool()
px.display(df)
)pxl";
  ASSERT_OK_AND_ASSIGN(auto graph, CompileGraph(query, {}, {{"tmp", moduleUsesGlobal}}));
  auto maps = graph->FindNodesThatMatch(Map());
  auto map = static_cast<MapIR*>(maps[0]);
  EXPECT_MATCH(map->col_exprs()[0].node, Bool(true));
}

TEST_F(ASTVisitorTest, writing_to_global_variable_throws_error) {
  EXPECT_COMPILER_ERROR(CompileGraph("True = 'blah'", {}, {}).status(),
                        ".*?can't assign to True.*?");
  EXPECT_COMPILER_ERROR(CompileGraph("False = 'blah'", {}, {}).status(),
                        ".*?can't assign to False.*?");
  EXPECT_COMPILER_ERROR(CompileGraph("None = 'blah'", {}, {}).status(),
                        ".*?can't assign to None.*?");
  // AugAssign
  EXPECT_COMPILER_ERROR(CompileGraph("True += 'blah'", {}, {}).status(),
                        ".*?can't assign to True.*?");
  EXPECT_COMPILER_ERROR(CompileGraph("False += 'blah'", {}, {}).status(),
                        ".*?can't assign to False.*?");
  EXPECT_COMPILER_ERROR(CompileGraph("None += 'blah'", {}, {}).status(),
                        ".*?can't assign to None.*?");
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
