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
TEST(ASTVisitor, compilation_test) {
  std::string from_expr = "dataframe(table='cpu', select=['cpu0', 'cpu1'])";
  auto ig_status = ParseQuery(from_expr);
  EXPECT_OK(ig_status);
  auto ig = ig_status.ValueOrDie();
  // check the connection of ig
  std::string from_range_expr = "dataframe(table='cpu', select=['cpu0']).range(start=0,stop=10)";
  EXPECT_OK(ParseQuery(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST(ASTVisitor, extra_arguments) {
  std::string extra_from_args =
      "dataframe(table='cpu', select=['cpu0'], fakeArg='hahaha').range(start=0,stop=10)";
  Status s1 = ParseQuery(extra_from_args).status();
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
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 10);
  EXPECT_THAT(s1, HasCompilerError("dataframe.* got an unexpected keyword argument 'fakeArg'"));
}

TEST(ASTVisitor, missing_one_argument) {
  std::string missing_from_args = "dataframe(select=['cpu']).range(start=0,stop=10)";
  Status s2 = ParseQuery(missing_from_args).status();
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
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 10);
  EXPECT_THAT(s2,
              HasCompilerError("dataframe.* missing 1 required positional argument.*? 'table'"));
}

TEST(ASTVisitor, from_select_default_arg) {
  std::string no_select_arg = "dataframe(table='cpu').result(name='out')";
  EXPECT_OK(ParseQuery(no_select_arg));
}

TEST(ASTVisitor, positional_args) {
  std::string positional_arg = "dataframe('cpu').result(name='out')";
  EXPECT_OK(ParseQuery(positional_arg));
}

// Checks to make sure the parser identifies bad syntax
TEST(ASTVisitor, bad_syntax) {
  std::string early_paranetheses_close = "dataframe";
  EXPECT_FALSE(ParseQuery(early_paranetheses_close).ok());
}
// Checks to make sure the compiler can catch operators that don't exist.
TEST(ASTVisitor, nonexistant_operator_names) {
  std::string wrong_from_op_name =
      "notdataframe(table='cpu', select=['cpu0']).range(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(wrong_from_op_name).ok());
  std::string wrong_range_op_name =
      "dataframe(table='cpu', select=['cpu0']).brange(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(wrong_range_op_name).ok());
}
TEST(ASTVisitor, assign_functionality) {
  std::string simple_assign = "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1'])";
  EXPECT_OK(ParseQuery(simple_assign));
  std::string assign_and_use =
      absl::StrJoin({"queryDF = dataframe(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
                     "queryDF.range(start=0,stop=10)"},
                    "\n");
  EXPECT_OK(ParseQuery(assign_and_use));
}
TEST(ASTVisitor, assign_error_checking) {
  std::string bad_assign_mult_values = absl::StrJoin(
      {
          "queryDF,haha = dataframe(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF.range(start=0,stop=10)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(bad_assign_mult_values).ok());
  std::string bad_assign_str = "queryDF = 'str'";
  EXPECT_FALSE(ParseQuery(bad_assign_str).ok());
}
// Map Tests
TEST(MapTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "rangeDF = queryDF[['sum']]",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sum));
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['div'] = pl.div(queryDF['cpu0'], queryDF['cpu1'])",
          "rangeDF = queryDF[['div']]",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_div_map_query));
}

TEST(MapTest, single_col_map_subscript) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['cpu2'] = queryDF['cpu0'] + queryDF['cpu1']",
      },
      "\n");
  auto result_s = ParseQuery(single_col_map_sum);
  EXPECT_OK(result_s);
  auto result = result_s.ValueOrDie();

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

TEST(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
          "queryDF['copy'] = queryDF['cpu2']",
      },
      "\n");
  EXPECT_OK(ParseQuery(multi_col));
}

TEST(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sum));
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sub));
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['product'] = queryDF['cpu0'] * queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_product));
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['quotient'] = queryDF['cpu0'] / queryDF['cpu1']",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_quotient));
}

TEST(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1'] + queryDF['cpu2']",
      },
      "\n");
  EXPECT_OK(ParseQuery(nested_expr));
  std::string nested_fn = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['div'] = pl.div(queryDF['cpu0'] + queryDF['cpu1'], queryDF['cpu2'])",
      },
      "\n");
  EXPECT_OK(ParseQuery(nested_fn));
}

TEST(MapTest, wrong_df_name) {
  std::string wrong_df = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "wrong = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sum'] = wrong['cpu0'] + wrong['cpu1'] + wrong['cpu2']",
      },
      "\n");
  auto ir_graph_status = ParseQuery(wrong_df);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("name 'wrong' is not available in this context"));
}

TEST(MapTest, missing_df) {
  std::string wrong_df = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "queryDF['sum'] = dne['cpu0'] + dne['cpu1'] + dne['cpu2']",
      },
      "\n");
  auto ir_graph_status = ParseQuery(wrong_df);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("name 'dne' is not available in this context"));
}

TEST(AggTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', "
          "'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=lambda r : "
          "{'cpu_count' : "
          "pl.count(r.cpu1)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_agg));
  std::string multi_output_col_agg = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
       "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)})"},
      "\n");
  EXPECT_OK(ParseQuery(multi_output_col_agg));
  std::string multi_input_col_agg = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).range(start=0,stop=10)",
       "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), "
       "'cpu2_mean' : pl.mean(r.cpu2)})"},
      "\n");
  EXPECT_OK(ParseQuery(multi_input_col_agg));
}

TEST(AggTest, not_allowed_by) {
  std::string single_col_bad_by_fn = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : r, fn=lambda r :  {'cpu_count' : "
          "pl.count(r.cpu0)})",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_by_fn).ok());
  std::string single_col_bad_by_attr = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=pl.mean, fn={'cpu_count' : pl.count(r.cpu0)})",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_by_attr).ok());
}
TEST(AggTest, not_allowed_agg_fn) {
  std::string single_col_bad_agg_fn = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=1+2)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_agg_fn).ok());
  std::string single_col_dict_by_not_pl = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=notpl.count)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_dict_by_not_pl).ok());
  std::string single_col_dict_by_no_attr_fn = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r :r.cpu0, fn=count)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_dict_by_no_attr_fn).ok());
  std::string valid_fn_not_valid_call = absl::StrJoin(
      {"queryDF = dataframe(table = 'cpu', select = [ 'cpu0', 'cpu1' ]).range(time = '-2m')",
       "rangeDF =queryDF.agg(by = lambda r: r.cpu0, fn = pl.count) "},
      "\n");
  EXPECT_FALSE(ParseQuery(valid_fn_not_valid_call).ok());
}

TEST(ResultTest, basic) {
  std::string single_col_map_sub = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
       "queryDF['sub'] = queryDF['cpu0'] - queryDF['cpu1']",
       "result = queryDF[['sub']].result(name='mapped')"},
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sub));
}

TEST(OptionalArgs, group_by_all) {
  std::string agg_query =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF.agg(fn=lambda r : {'sum' : pl.sum(r.cpu0)}).result(name='agg')"},
                    "\n");
  EXPECT_OK(ParseQuery(agg_query));
}

TEST(OptionalArgs, DISABLED_map_copy_relation) {
  // TODO(philkuz) later diff impl this.
  // TODO(philkuz) make a relation handler test that confirms the relation is actually copied.

  std::string map_query = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1'])",
       "queryDF['sum'] = queryDF['cpu0'] + queryDF['cpu1']", "queryDF.result(name='map')"},
      "\n");
  EXPECT_OK(ParseQuery(map_query));
}

TEST(RangeValueTests, time_range_compilation) {
  // now doesn't accept args.
  std::string stop_expr = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                         "'cpu1']).range(start=0,stop=plc.now()-plc.seconds(2))",
                                         "queryDF.result(name='mapped')"},
                                        "\n");
  EXPECT_OK(ParseQuery(stop_expr));

  std::string start_and_stop_expr = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', "
       "'cpu1']).range(start=plc.now() - plc.minutes(2),stop=plc.now()-plc.seconds(2))",
       "queryDF.result(name='mapped')"},
      "\n");
  EXPECT_OK(ParseQuery(start_and_stop_expr));
}

TEST(RangeValueTests, implied_stop_params) {
  std::string start_expr_only = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                               "'cpu1']).range(start=plc.now() - plc.minutes(2))",
                                               "queryDF.result(name='mapped')"},
                                              "\n");
  EXPECT_OK(ParseQuery(start_expr_only));
}

TEST(RangeValueTests, string_start_param) {
  // TODO(philkuz) make a paramtereized test that takes in a value for minutes and makes sure they
  // all compile correctly.
  std::string start_expr_only = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                               "'cpu1']).range(start='-2m')",
                                               "queryDF.result(name='mapped')"},
                                              "\n");
  EXPECT_OK(ParseQuery(start_expr_only));
}

class FilterTestParam : public ::testing::TestWithParam<std::string> {
 protected:
  void SetUp() {
    // TODO(philkuz) use Combine with the tuple to get out a set of different values for each of the
    // values.
    compare_op_ = GetParam();
    query = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                           "'cpu1']).filter(fn=lambda r : r.cpu0 $0 0.5)",
                           "queryDF.result(name='filtered')"},
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

TEST(FilterExprTest, basic) {
  // Test for and
  std::string simple_and =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                     "'cpu1']).filter(fn=lambda r : r.cpu0 == 0.5 and r.cpu1 >= 0.2)",
                     "queryDF.result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(simple_and));
  // Test for or
  std::string simple_or =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                     "'cpu1']).filter(fn=lambda r : r.cpu0 == 0.5 or r.cpu1 >= 0.2)",
                     "queryDF.result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(simple_or));
  // Test for nested and/or clauses
  std::string and_or_query =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                     "'cpu1']).filter(fn=lambda r : r.cpu0 == 0.5 and r.cpu1 "
                     ">= 0.2 or r.cpu0 >= 0.5 and r.cpu1 == 0.2)",
                     "queryDF.result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(and_or_query));
  // TODO(philkuz) check that and/or clauses are honored properly.
  // TODO(philkuz) handle simple math opes
}

TEST(LimitTest, basic) {
  std::string limit = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                     "'cpu1']).limit(rows=100)",
                                     "queryDF.result(name='limited')"},
                                    "\n");
  EXPECT_OK(ParseQuery(limit));
}

TEST(LimitTest, limit_invalid_queries) {
  std::string no_arg = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                      "'cpu1']).limit()",
                                      "queryDF.result(name='limited')"},
                                     "\n");
  // No arg shouldn't work.
  EXPECT_NOT_OK(ParseQuery(no_arg));

  std::string string_arg = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                          "'cpu1']).limit(rows='arg')",
                                          "queryDF.result(name='limited')"},
                                         "\n");
  // String as an arg should not work.
  EXPECT_NOT_OK(ParseQuery(string_arg));

  std::string float_arg = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                         "'cpu1']).limit(rows=1.2)",
                                         "queryDF.result(name='limited')"},
                                        "\n");
  // float as an arg should not work.
  EXPECT_NOT_OK(ParseQuery(float_arg));
}

TEST(FilterTest, filter_invalid_queries) {
  std::string int_val = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                       "'cpu1']).filter(fn=1)",
                                       "queryDF.result(name='filtered')"},
                                      "\n");
  EXPECT_NOT_OK(ParseQuery(int_val));
}

// TODO(philkuz) (PL-524) both of these changes require modifications to the actual parser.
TEST(NegationTest, DISABLED_bang_negation) {
  std::string bang_negation =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['bool_col']) "
                     "filterDF = queryDF.filter(fn=lambda r : !r.bool_col)",
                     "filterDF.result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(bang_negation));
}

TEST(NegationTest, DISABLED_pythonic_negation) {
  std::string pythonic_negation =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['bool_col']) "
                     "filterDF = queryDF.filter(fn=lambda r : not r.bool_col)",
                     "filterDF.result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(pythonic_negation));
}
class OpsAsAttributes : public ::testing::TestWithParam<std::string> {};
TEST_P(OpsAsAttributes, valid_attributes) {
  std::string op_call = GetParam();
  std::string invalid_query =
      absl::StrJoin({"invalid_queryDF = dataframe(table='cpu', select=['bool_col']) ", "opDF = $0",
                     "opDF.result(name='out')"},
                    "\n");
  invalid_query = absl::Substitute(invalid_query, op_call);
  EXPECT_NOT_OK(ParseQuery(invalid_query));
  std::string valid_query = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['bool_col']) ",
                                           "opDF = queryDF.$0", "opDF.result(name='out')"},
                                          "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  EXPECT_OK(ParseQuery(valid_query));
}
std::vector<std::string> operators{
    "filter(fn=lambda r : r.bool_col)", "map(fn=lambda r : {'boolin': r.bool_col})",
    "agg(fn=lambda r : {'count': pl.count(r.bool_col)},by=lambda r : r.bool_col)",
    "limit(rows=1000)", "range(start=plc.now() - plc.minutes(2), stop=plc.now())"};

INSTANTIATE_TEST_SUITE_P(OpsAsAttributesSuite, OpsAsAttributes, ::testing::ValuesIn(operators));

class MetadataAttributes : public ::testing::TestWithParam<std::string> {};
TEST_P(MetadataAttributes, valid_metadata_calls) {
  std::string op_call = GetParam();
  std::string valid_query =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['bool_col', 'not_bool_col']) ",
                     "opDF = queryDF.$0", "opDF.result(name='out')"},
                    "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  VLOG(1) << valid_query;
  EXPECT_OK(ParseQuery(valid_query));
}
std::vector<std::string> metadata_operators{
    "filter(fn=lambda r : r.attr.services == 'orders')",
    "map(fn=lambda r : {'services': r.attr.services})",
    "agg(fn=lambda r : {'count': pl.count(r.bool_col)},by=lambda r : r.attr.services)",
    "agg(fn=lambda r : {'count': pl.count(r.bool_col)},by=lambda r : [r.not_bool_col, "
    "r.attr.services])"};

INSTANTIATE_TEST_SUITE_P(MetadataAttributesSuite, MetadataAttributes,
                         ::testing::ValuesIn(metadata_operators));

TEST(MetadataAttributes, metadata_columns_added) {
  std::string valid_query =
      absl::StrJoin({"queryDF = dataframe(table='cpu', select=['bool_col', 'not_bool_col']) ",
                     "opDF = queryDF.agg(fn=lambda r :{'count': pl.count(r.bool_col)},by=lambda r "
                     ": r.attr.services)",
                     "opDF.result(name='out')"},
                    "\n");
  VLOG(1) << valid_query;
  std::shared_ptr<IR> graph = ParseQuery(valid_query).ValueOrDie();
  IRNode* current_node = nullptr;
  for (const auto& id : graph->dag().TopologicalSort()) {
    current_node = graph->Get(id);
    if (current_node->type() == IRNodeType::kBlockingAgg) {
      break;
    }
  }
  ASSERT_NE(current_node, nullptr);
  ASSERT_TRUE(current_node->type() == IRNodeType::kBlockingAgg)
      << absl::Substitute("Found $0 instead of BlockingAGg", current_node->type_string());

  BlockingAggIR* agg_node = static_cast<BlockingAggIR*>(current_node);
  std::vector<ColumnIR*> groups = agg_node->groups();
  EXPECT_EQ(groups.size(), 1UL);
  EXPECT_EQ(groups[0]->type(), IRNodeType::kMetadata);
  MetadataIR* metadata_node = static_cast<MetadataIR*>(groups[0]);
  EXPECT_EQ(metadata_node->name(), "services");
}

TEST(MetadataAttributes, nested_attribute_logical_errors) {
  // I originally had a  weird design where the first value in an attribute chain
  // (<first_value>.<second_value>.<third_value>) would be evaluated after the second.
  // Making sure that doesn't happern here.
  std::string valid_query = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['bool_col', 'not_bool_col']) ",
       "opDF = queryDF.agg(fn=lambda r : pl.count(r.bool_col),by=lambda r : pl.attr.services)",
       "opDF.result(name='out')"},
      "\n");
  VLOG(1) << valid_query;
  auto failed_query_status = ParseQuery(valid_query);
  EXPECT_NOT_OK(failed_query_status);
  VLOG(1) << failed_query_status.status().ToString();
  EXPECT_THAT(
      failed_query_status.status(),
      HasCompilerError(
          "Metadata call not allowed on \'pl\'. Must use a lambda argument to access Metadata."));
}
TEST(AggTest, not_allowed_by_arguments) {
  std::string single_col_bad_by_fn_expr = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : 1+2, fn=lambda r: {'cpu_count' : "
          "pl.count(r.cpu0)}).result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(single_col_bad_by_fn_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_NOT_OK(ir_graph_status);

  std::string single_col_dict_by_fn = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : {'cpu' : r.cpu0}, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu0)}).result(name='cpu2')",
      },
      "\n");

  ir_graph_status = ParseQuery(single_col_dict_by_fn);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_NOT_OK(ir_graph_status);
}

const char* kJoinDuplicatedLambdaQuery = R"query(
src1 = dataframe(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = dataframe(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.merge(src2,  type='inner',
                      cond=lambda r, r: r1.upid == r2.upid,
                      cols=lambda r1, r2: {
                        'upid': r1.upid,
                        'bytes_in': r2.bytes_in,
                        'bytes_out': r2.bytes_out,
                        'cpu0': r1.cpu0,
                        'cpu1': r1.cpu1,
                      })
join.result(name='joined')
)query";

TEST(LambdaTest, duplicate_arguments) {
  auto ir_graph_status = ParseQuery(kJoinDuplicatedLambdaQuery);
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("Duplicate argument 'r' in lambda definition."));
}

TEST(RangeTest, test_parent_is_memory_source) {
  std::string add_combination = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF['foo'] = 2",
          "queryDF.range(start=10, stop=12).result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(add_combination);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("Expected parent of Range to be a Memory Source, not a Map."));
}

const char* kInnerJoinQuery = R"query(
src1 = dataframe(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = dataframe(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.merge(src2,  type='inner',
                      cond=lambda r1, r2: r1.upid == r2.upid,
                      cols=lambda r1, r2: {
                        'upid': r1.upid,
                        'bytes_in': r2.bytes_in,
                        'bytes_out': r2.bytes_out,
                        'cpu0': r1.cpu0,
                        'cpu1': r1.cpu1,
                      })
join.result(name='joined')
)query";

TEST(JoinTest, test_inner_join) {
  auto ir_graph_status = ParseQuery(kInnerJoinQuery);
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

  EXPECT_THAT(join->column_names(), ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));

  std::vector<std::string> column_ir_names;
  std::vector<int64_t> column_ir_parent_idx;

  for (ColumnIR* col : join->output_columns()) {
    column_ir_names.push_back(col->col_name());
    column_ir_parent_idx.push_back(col->container_op_parent_idx());
  }

  EXPECT_THAT(column_ir_names, ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));
  EXPECT_THAT(column_ir_parent_idx, ElementsAre(0, 1, 1, 0, 0));

  EXPECT_EQ(join->join_type(), JoinIR::JoinType::kInner);
  EXPECT_THAT(graph->dag().ParentsOf(join->id()), ElementsAre(mem_src1->id(), mem_src2->id()));
}

const char* kNewFilterQuery = R"query(
df = dataframe("bar")
df = df[df["service"] == "foo"]
df.result("ld")
)query";

TEST(FilterTest, TestNewFilter) {
  auto ir_graph_or_s = ParseQuery(kNewFilterQuery);
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
df = dataframe("bar")
df[df["service"] == "foo"].result("ld")
)query";

TEST(FilterTest, ChainedFilterQuery) {
  auto ir_graph_or_s = ParseQuery(kFilterChainedQuery);
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
df = dataframe("bar")[df["service"] == "foo"].result("ld")
)query";

// Filter can't be defined when it's chained after a node.
TEST(FilterTest, InvalidChainedFilterQuery) {
  auto ir_graph_or_s = ParseQuery(kInvalidFilterChainQuery);
  ASSERT_NOT_OK(ir_graph_or_s);
  EXPECT_THAT(ir_graph_or_s.status(),
              HasCompilerError("name 'df' is not available in this context"));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
