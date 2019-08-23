#include "src/carnot/compiler/ast_visitor.h"
#include <gtest/gtest.h>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/ir_test_utils.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

// Checks whether we can actually compile into a graph.
TEST(ASTVisitor, compilation_test) {
  std::string from_expr = "From(table='cpu', select=['cpu0', 'cpu1'])";
  auto ig_status = ParseQuery(from_expr);
  EXPECT_OK(ig_status);
  auto ig = ig_status.ValueOrDie();
  VerifyGraphConnections(ig.get());
  // check the connection of ig
  std::string from_range_expr = "From(table='cpu', select=['cpu0']).Range(start=0,stop=10)";
  EXPECT_OK(ParseQuery(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST(ASTVisitor, extra_arguments) {
  std::string extra_from_args =
      "From(table='cpu', select=['cpu0'], fakeArg='hahaha').Range(start=0,stop=10)";
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
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 5);
  EXPECT_EQ(error_group.errors(0).line_col_error().message(),
            "Keyword \'fakeArg\' not expected in function.");
}

TEST(ASTVisitor, missing_one_argument) {
  std::string missing_from_args = "From(select=['cpu']).Range(start=0,stop=10)";
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
  EXPECT_EQ(error_group.errors(0).line_col_error().column(), 5);
  EXPECT_EQ(error_group.errors(0).line_col_error().message(),
            "You must set \'table\' directly. No default value found.");
}

TEST(ASTVisitor, from_select_default_arg) {
  std::string no_select_arg = "From(table='cpu').Result(name='out')";
  EXPECT_OK(ParseQuery(no_select_arg));
}

// Checks to make sure the parser identifies bad syntax
TEST(ASTVisitor, bad_syntax) {
  std::string early_paranetheses_close = "From";
  EXPECT_FALSE(ParseQuery(early_paranetheses_close).ok());
}
// Checks to make sure the compiler can catch operators that don't exist.
TEST(ASTVisitor, nonexistant_operator_names) {
  std::string wrong_from_op_name = "Drom(table='cpu', select=['cpu0']).Range(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(wrong_from_op_name).ok());
  std::string wrong_range_op_name = "From(table='cpu', select=['cpu0']).BRange(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(wrong_range_op_name).ok());
}
TEST(ASTVisitor, assign_functionality) {
  std::string simple_assign = "queryDF = From(table='cpu', select=['cpu0', 'cpu1'])";
  EXPECT_OK(ParseQuery(simple_assign));
  std::string assign_and_use =
      absl::StrJoin({"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
                     "queryDF.Range(start=0,stop=10)"},
                    "\n");
  EXPECT_OK(ParseQuery(assign_and_use));
}
TEST(ASTVisitor, assign_error_checking) {
  std::string bad_assign_mult_values = absl::StrJoin(
      {
          "queryDF,haha = From(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF.Range(start=0,stop=10)",
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
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sum));
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0,r.cpu1)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_div_map_query));
}

TEST(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : r.cpu2})",
      },
      "\n");
  EXPECT_OK(ParseQuery(multi_col));
}

TEST(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sum));
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sub));
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'product' : r.cpu0 * r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_product));
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_quotient));
}

TEST(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1 + r.cpu2})",
      },
      "\n");
  EXPECT_OK(ParseQuery(nested_expr));
  std::string nested_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0 + r.cpu1, r.cpu2)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(nested_fn));
}

TEST(AggTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', "
          "'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : "
          "{'cpu_count' : "
          "pl.count(r.cpu1)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_agg));
  std::string multi_output_col_agg =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
                     "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)})"},
                    "\n");
  EXPECT_OK(ParseQuery(multi_output_col_agg));
  std::string multi_input_col_agg = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0,stop=10)",
       "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), "
       "'cpu2_mean' : pl.mean(r.cpu2)})"},
      "\n");
  EXPECT_OK(ParseQuery(multi_input_col_agg));
}

TEST(AggTest, not_allowed_by) {
  std::string single_col_bad_by_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r, fn=lambda r :  {'cpu_count' : "
          "pl.count(r.cpu0)})",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_by_fn).ok());
  std::string single_col_bad_by_attr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=pl.mean, fn={'cpu_count' : pl.count(r.cpu0)})",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_by_attr).ok());
}
TEST(AggTest, not_allowed_agg_fn) {
  std::string single_col_bad_agg_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=1+2)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_agg_fn).ok());
  std::string single_col_dict_by_not_pl = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=notpl.count)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_dict_by_not_pl).ok());
  std::string single_col_dict_by_no_attr_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r :r.cpu0, fn=count)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_dict_by_no_attr_fn).ok());
  std::string valid_fn_not_valid_call = absl::StrJoin(
      {"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ]).Range(time = '-2m')",
       "rangeDF =queryDF.Agg(by = lambda r: r.cpu0, fn = pl.count) "},
      "\n");
  EXPECT_FALSE(ParseQuery(valid_fn_not_valid_call).ok());
}

TEST(ResultTest, basic) {
  std::string single_col_map_sub =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
                     "rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})",
                     "result = rangeDF.Result(name='mapped')"},
                    "\n");
  EXPECT_OK(ParseQuery(single_col_map_sub));
}

TEST(OptionalArgs, group_by_all) {
  std::string agg_query =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF.Agg(fn=lambda r : {'sum' : pl.sum(r.cpu0)}).Result(name='agg')"},
                    "\n");
  EXPECT_OK(ParseQuery(agg_query));
}

TEST(OptionalArgs, DISABLED_map_copy_relation) {
  // TODO(philkuz) later diff impl this.
  // TODO(philkuz) make a relation handler test that confirms the relation is actually copied.

  std::string map_query = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
                                         "queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1}, "
                                         "copy_source_cols=True).Result(name='map')"},
                                        "\n");
  EXPECT_OK(ParseQuery(map_query));
}

TEST(RangeValueTests, now_should_compile_without_args) {
  std::string plc_now_test = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=plc.now())",
       "queryDF.Result(name='mapped')"},
      "\n");
  EXPECT_OK(ParseQuery(plc_now_test));
}

TEST(RangeValueTests, now_should_fail_with_args) {
  // now doesn't accept args.
  std::string now_with_args = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=plc.now(1))",
       "queryDF.Result(name='mapped')"},
      "\n");
  auto status = ParseQuery(now_with_args);
  VLOG(2) << status.ToString();
  EXPECT_FALSE(status.ok());
}

TEST(RangeValueTests, time_range_compilation) {
  // now doesn't accept args.
  std::string stop_expr = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                         "'cpu1']).Range(start=0,stop=plc.now()-plc.seconds(2))",
                                         "queryDF.Result(name='mapped')"},
                                        "\n");
  EXPECT_OK(ParseQuery(stop_expr));

  std::string start_and_stop_expr = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', "
       "'cpu1']).Range(start=plc.now() - plc.minutes(2),stop=plc.now()-plc.seconds(2))",
       "queryDF.Result(name='mapped')"},
      "\n");
  EXPECT_OK(ParseQuery(start_and_stop_expr));
}

TEST(RangeValueTests, nonexistant_time_variables) {
  // now doesn't accept args.
  std::string start_expr = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                          "'cpu1']).Range(start=0,stop=plc.now()-plc.nevers(2))",
                                          "queryDF.Result(name='mapped')"},
                                         "\n");
  EXPECT_NOT_OK(ParseQuery(start_expr));

  std::string start_and_stop_expr =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                     "'cpu1']).Range(start=plc.notnow(),stop=plc.now()-plc.nevers(2))",
                     "queryDF.Result(name='mapped')"},
                    "\n");
  EXPECT_NOT_OK(ParseQuery(start_and_stop_expr));
}

TEST(RangeValueTests, namespace_mismatch) {
  std::string start_and_stop_expr =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                     "'cpu1']).Range(start=pl.now() - pl.minutes(2),stop=pl.now()-pl.seconds(2))",
                     "queryDF.Result(name='mapped')"},
                    "\n");
  EXPECT_NOT_OK(ParseQuery(start_and_stop_expr));
}

TEST(RangeValueTests, implied_stop_params) {
  std::string start_expr_only = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                               "'cpu1']).Range(start=plc.now() - plc.minutes(2))",
                                               "queryDF.Result(name='mapped')"},
                                              "\n");
  EXPECT_OK(ParseQuery(start_expr_only));
}

TEST(RangeValueTests, string_start_param) {
  // TODO(philkuz) make a paramtereized test that takes in a value for minutes and makes sure they
  // all compile correctly.
  std::string start_expr_only = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                               "'cpu1']).Range(start='-2m')",
                                               "queryDF.Result(name='mapped')"},
                                              "\n");
  EXPECT_OK(ParseQuery(start_expr_only));
}

class FilterTestParam : public ::testing::TestWithParam<std::string> {
 protected:
  void SetUp() {
    // TODO(philkuz) use Combine with the tuple to get out a set of different values for each of the
    // values.
    compare_op_ = GetParam();
    query = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                           "'cpu1']).Filter(fn=lambda r : r.cpu0 $0 0.5)",
                           "queryDF.Result(name='filtered')"},
                          "\n");
    query = absl::Substitute(query, compare_op_);
    VLOG(2) << query;
  }
  std::string compare_op_;
  std::string query;
};

std::vector<std::string> comparison_functions = {">", "<", "==", ">=", "<="};

TEST_P(FilterTestParam, filter_simple_ops_test) { EXPECT_OK(ParseQuery(query)); }

INSTANTIATE_TEST_CASE_P(FilterTestSuites, FilterTestParam,
                        ::testing::ValuesIn(comparison_functions));

TEST(FilterExprTest, basic) {
  // Test for and
  std::string simple_and =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                     "'cpu1']).Filter(fn=lambda r : r.cpu0 == 0.5 and r.cpu1 >= 0.2)",
                     "queryDF.Result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(simple_and));
  // Test for or
  std::string simple_or =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                     "'cpu1']).Filter(fn=lambda r : r.cpu0 == 0.5 or r.cpu1 >= 0.2)",
                     "queryDF.Result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(simple_or));
  // Test for nested and/or clauses
  std::string and_or_query =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                     "'cpu1']).Filter(fn=lambda r : r.cpu0 == 0.5 and r.cpu1 "
                     ">= 0.2 or r.cpu0 >= 0.5 and r.cpu1 == 0.2)",
                     "queryDF.Result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(and_or_query));
  // TODO(philkuz) check that and/or clauses are honored properly.
  // TODO(philkuz) handle simple math opes
}

TEST(LimitTest, basic) {
  std::string limit = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                     "'cpu1']).Limit(rows=100)",
                                     "queryDF.Result(name='limited')"},
                                    "\n");
  EXPECT_OK(ParseQuery(limit));
}

TEST(LimitTest, limit_invalid_queries) {
  std::string no_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                      "'cpu1']).Limit()",
                                      "queryDF.Result(name='limited')"},
                                     "\n");
  // No arg shouldn't work.
  EXPECT_NOT_OK(ParseQuery(no_arg));

  std::string string_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                          "'cpu1']).Limit(rows='arg')",
                                          "queryDF.Result(name='limited')"},
                                         "\n");
  // String as an arg should not work.
  EXPECT_NOT_OK(ParseQuery(string_arg));

  std::string float_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                         "'cpu1']).Limit(rows=1.2)",
                                         "queryDF.Result(name='limited')"},
                                        "\n");
  // float as an arg should not work.
  EXPECT_NOT_OK(ParseQuery(float_arg));
}

TEST(FilterTest, filter_invalid_queries) {
  std::string int_val = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                       "'cpu1']).Filter(fn=1)",
                                       "queryDF.Result(name='filtered')"},
                                      "\n");
  EXPECT_NOT_OK(ParseQuery(int_val));
}

// TODO(philkuz) (PL-524) both of these changes require modifications to the actual parser.
TEST(NegationTest, DISABLED_bang_negation) {
  std::string bang_negation = absl::StrJoin({"queryDF = From(table='cpu', select=['bool_col']) "
                                             "filterDF = queryDF.Filter(fn=lambda r : !r.bool_col)",
                                             "filterDF.Result(name='filtered')"},
                                            "\n");
  EXPECT_OK(ParseQuery(bang_negation));
}

TEST(NegationTest, DISABLED_pythonic_negation) {
  std::string pythonic_negation =
      absl::StrJoin({"queryDF = From(table='cpu', select=['bool_col']) "
                     "filterDF = queryDF.Filter(fn=lambda r : not r.bool_col)",
                     "filterDF.Result(name='filtered')"},
                    "\n");
  EXPECT_OK(ParseQuery(pythonic_negation));
}
class OpsAsAttributes : public ::testing::TestWithParam<std::string> {};
TEST_P(OpsAsAttributes, valid_attributes) {
  std::string op_call = GetParam();
  std::string invalid_query =
      absl::StrJoin({"invalid_queryDF = From(table='cpu', select=['bool_col']) ", "opDF = $0",
                     "opDF.Result(name='out')"},
                    "\n");
  invalid_query = absl::Substitute(invalid_query, op_call);
  EXPECT_NOT_OK(ParseQuery(invalid_query));
  std::string valid_query = absl::StrJoin({"queryDF = From(table='cpu', select=['bool_col']) ",
                                           "opDF = queryDF.$0", "opDF.Result(name='out')"},
                                          "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  EXPECT_OK(ParseQuery(valid_query));
}
std::vector<std::string> operators{
    "Filter(fn=lambda r : r.bool_col)", "Map(fn=lambda r : {'boolin': r.bool_col})",
    "Agg(fn=lambda r : {'count': pl.count(r.bool_col)},by=lambda r : r.bool_col)",
    "Limit(rows=1000)", "Range(start=plc.now() - plc.minutes(2), stop=plc.now())"};

INSTANTIATE_TEST_CASE_P(OpsAsAttributesSuite, OpsAsAttributes, ::testing::ValuesIn(operators));

class MetadataAttributes : public ::testing::TestWithParam<std::string> {};
TEST_P(MetadataAttributes, valid_metadata_calls) {
  std::string op_call = GetParam();
  std::string valid_query =
      absl::StrJoin({"queryDF = From(table='cpu', select=['bool_col', 'not_bool_col']) ",
                     "opDF = queryDF.$0", "opDF.Result(name='out')"},
                    "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  VLOG(1) << valid_query;
  EXPECT_OK(ParseQuery(valid_query));
}
std::vector<std::string> metadata_operators{
    "Filter(fn=lambda r : r.attr.services == 'orders')",
    "Map(fn=lambda r : {'services': r.attr.services})",
    "Agg(fn=lambda r : {'count': pl.count(r.bool_col)},by=lambda r : r.attr.services)",
    "Agg(fn=lambda r : {'count': pl.count(r.bool_col)},by=lambda r : [r.not_bool_col, "
    "r.attr.services])"};

INSTANTIATE_TEST_CASE_P(MetadataAttributesSuite, MetadataAttributes,
                        ::testing::ValuesIn(metadata_operators));

TEST(MetadataAttributes, metadata_columns_added) {
  std::string valid_query =
      absl::StrJoin({"queryDF = From(table='cpu', select=['bool_col', 'not_bool_col']) ",
                     "opDF = queryDF.Agg(fn=lambda r :{'count': pl.count(r.bool_col)},by=lambda r "
                     ": r.attr.services)",
                     "opDF.Result(name='out')"},
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
      {"queryDF = From(table='cpu', select=['bool_col', 'not_bool_col']) ",
       "opDF = queryDF.Agg(fn=lambda r : pl.count(r.bool_col),by=lambda r : pl.attr.services)",
       "opDF.Result(name='out')"},
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
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : 1+2, fn=lambda r: {'cpu_count' : "
          "pl.count(r.cpu0)}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(single_col_bad_by_fn_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_NOT_OK(ir_graph_status);

  std::string single_col_dict_by_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : {'cpu' : r.cpu0}, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu0)}).Result(name='cpu2')",
      },
      "\n");

  ir_graph_status = ParseQuery(single_col_dict_by_fn);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_NOT_OK(ir_graph_status);
}

TEST(AggTest, nested_agg_expression_should_fail) {
  std::string nested_agg_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0, stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
          "pl.sum(pl.mean(r.cpu0))}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(nested_agg_fn);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);

  std::string add_combination = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0, stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
          "pl.mean(r.cpu0)+2}).Result(name='cpu2')",
      },
      "\n");
  ir_graph_status = ParseQuery(add_combination);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
}

TEST(LambdaTest, test_wrong_number_of_arguments) {
  std::string add_combination = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF.Map(fn=lambda r, b: {'cpu_plus_2' : r.cpu0+2}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(add_combination);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("Got 2 lambda arguments, expected 1 for the Map Operator."));
}

const char* kJoinDuplicatedLambdaQuery = R"query(
src1 = From(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = From(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.Join(src2,  type='inner',
                      cond=lambda r, r: r1.upid == r2.upid,
                      cols=lambda r1, r2: {
                        'upid': r1.upid,
                        'bytes_in': r2.bytes_in,
                        'bytes_out': r2.bytes_out,
                        'cpu0': r1.cpu0,
                        'cpu1': r1.cpu1,
                      })
join.Result(name='joined')
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
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF.Map(fn=lambda r: {'cpu_plus_2' : r.cpu0+2}).Range(start=10, "
          "stop=12).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(add_combination);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("Expected parent of Range to be a Memory Source, not a Map."));
}

const char* kInnerJoinQuery = R"query(
src1 = From(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = From(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.Join(src2,  type='inner',
                      cond=lambda r1, r2: r1.upid == r2.upid,
                      cols=lambda r1, r2: {
                        'upid': r1.upid,
                        'bytes_in': r2.bytes_in,
                        'bytes_out': r2.bytes_out,
                        'cpu0': r1.cpu0,
                        'cpu1': r1.cpu1,
                      })
join.Result(name='joined')
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

  EXPECT_TRUE(Match(join->condition_expr(), Equals(ColumnNode(), ColumnNode())));

  EXPECT_THAT(join->column_names(), ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));

  std::vector<std::string> column_ir_names;
  std::vector<int64_t> column_ir_parent_idx;

  for (ColumnIR* col : join->output_columns()) {
    column_ir_names.push_back(col->col_name());
    column_ir_parent_idx.push_back(col->container_op_parent_idx());
  }
  EXPECT_THAT(column_ir_names, ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));
  EXPECT_THAT(column_ir_parent_idx, ElementsAre(0, 1, 1, 0, 0));

  EXPECT_EQ(join->join_type(), planpb::JoinOperator_JoinType_INNER);
  EXPECT_THAT(graph->dag().ParentsOf(join->id()), ElementsAre(mem_src1->id(), mem_src2->id()));
}

const char* kJoinQueryTpl = R"query(
src1 = From(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = From(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.Join(src2,  type='$0',
                      cond=lambda r1, r2: r1.upid == r2.upid,
                      cols=lambda r1, r2: {
                        'upid': r1.upid,
                        'bytes_in': r2.bytes_in,
                        'bytes_out': r2.bytes_out,
                        'cpu0': r1.cpu0,
                        'cpu1': r1.cpu1,
                      })
join.Result(name='joined')
)query";

TEST(JoinTest, test_right_join) {
  auto ir_graph_status = ParseQuery(absl::Substitute(kJoinQueryTpl, "right"));
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
  // Right is swapped.
  EXPECT_THAT(join->parents(), ElementsAre(mem_src2, mem_src1));

  EXPECT_TRUE(Match(join->condition_expr(), Equals(ColumnNode(), ColumnNode())));

  EXPECT_THAT(join->column_names(), ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));

  std::vector<std::string> column_ir_names;
  std::vector<int64_t> column_ir_parent_idx;

  for (ColumnIR* col : join->output_columns()) {
    column_ir_names.push_back(col->col_name());
    column_ir_parent_idx.push_back(col->container_op_parent_idx());
  }
  EXPECT_THAT(column_ir_names, ElementsAre("upid", "bytes_in", "bytes_out", "cpu0", "cpu1"));
  EXPECT_THAT(column_ir_parent_idx, ElementsAre(1, 0, 0, 1, 1));

  EXPECT_EQ(join->join_type(), planpb::JoinOperator_JoinType_LEFT_OUTER);

  // test to make sure the order of the dag is consistent.
  EXPECT_THAT(graph->dag().ParentsOf(join->id()), ElementsAre(mem_src2->id(), mem_src1->id()));
}

TEST(JoinTest, bad_join_type) {
  auto ir_graph_status = ParseQuery(absl::Substitute(kJoinQueryTpl, "nothin"));
  ASSERT_NOT_OK(ir_graph_status);
  EXPECT_THAT(ir_graph_status.status(),
              HasCompilerError("'nothin' join type not supported. Only {inner,left,right,outer} "
                               "are available join types."));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
