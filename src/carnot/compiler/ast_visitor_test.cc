#include "src/carnot/compiler/ast_visitor.h"
#include <gtest/gtest.h>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ir_test_utils.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace compiler {

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
TEST(ASTVisitor, bad_arguments) {
  std::string extra_from_args =
      "From(table='cpu', select=['cpu0'], fakeArg='hahaha').Range(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(extra_from_args).ok());
  std::string missing_from_args = "From(table='cpu').Range(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(missing_from_args).ok());
  std::string no_from_args = "From().Range(start=0,stop=10)";
  EXPECT_FALSE(ParseQuery(no_from_args).ok());
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
TEST(TimeValueTest, basic) {
  std::string time_add_test =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
                     "rangeDF = queryDF.Map(fn=lambda r : {'time_add' : r.cpu0 + pl.second})",
                     "result = rangeDF.Result(name='mapped')"},
                    "\n");
  EXPECT_OK(ParseQuery(time_add_test));
  std::string bad_attribute_value =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
                     "rangeDF = queryDF.Map(fn=lambda r : {'time_add' : r.cpu0 + pl.notsecond})",
                     "result = rangeDF.Result(name='mapped')"},
                    "\n");
  EXPECT_FALSE(ParseQuery(bad_attribute_value).ok());
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
  VLOG(1) << status.ToString();
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
}  // namespace compiler

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
