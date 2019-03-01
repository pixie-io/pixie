#include "src/carnot/compiler/ast_visitor.h"
#include <gtest/gtest.h>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ir_test_utils.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/common.h"

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
  std::string from_range_expr = "From(table='cpu', select=['cpu0']).Range(time='-2m')";
  EXPECT_OK(ParseQuery(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST(ASTVisitor, bad_arguments) {
  std::string extra_from_args =
      "From(table='cpu', select=['cpu0'], fakeArg='hahaha').Range(time='-2m')";
  EXPECT_FALSE(ParseQuery(extra_from_args).ok());
  std::string missing_from_args = "From(table='cpu').Range(time='-2m')";
  EXPECT_FALSE(ParseQuery(missing_from_args).ok());
  std::string no_from_args = "From().Range(time='-2m')";
  EXPECT_FALSE(ParseQuery(no_from_args).ok());
}
// Checks to make sure the parser identifies bad syntax
TEST(ASTVisitor, bad_syntax) {
  std::string early_paranetheses_close = "From";
  EXPECT_FALSE(ParseQuery(early_paranetheses_close).ok());
}
// Checks to make sure the compiler can catch operators that don't exist.
TEST(ASTVisitor, nonexistant_operator_names) {
  std::string wrong_from_op_name = "Drom(table='cpu', select=['cpu0']).Range(time='-2m')";
  EXPECT_FALSE(ParseQuery(wrong_from_op_name).ok());
  std::string wrong_range_op_name = "From(table='cpu', select=['cpu0']).BRange(time='-2m')";
  EXPECT_FALSE(ParseQuery(wrong_range_op_name).ok());
}
TEST(ASTVisitor, assign_functionality) {
  std::string simple_assign = "queryDF = From(table='cpu', select=['cpu0', 'cpu1'])";
  EXPECT_OK(ParseQuery(simple_assign));
  std::string assign_and_use = absl::StrJoin(
      {"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ])", "queryDF.Range(time = '-2m')"},
      "\n");
  EXPECT_OK(ParseQuery(assign_and_use));
}
TEST(ASTVisitor, assign_error_checking) {
  std::string bad_assign_mult_values = absl::StrJoin(
      {
          "queryDF,haha = From(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF.Range(time='-2m')",
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
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sum));
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0,r.cpu1)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_div_map_query));
}

TEST(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : r.cpu2})",
      },
      "\n");
  EXPECT_OK(ParseQuery(multi_col));
}

TEST(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sum));
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_sub));
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'product' : r.cpu0 * r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_product));
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_map_quotient));
}

TEST(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1 + r.cpu2})",
      },
      "\n");
  EXPECT_OK(ParseQuery(nested_expr));
  std::string nested_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0 + r.cpu1, r.cpu2)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(nested_fn));
}

TEST(AggTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', "
          "'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : "
          "{'cpu_count' : "
          "pl.count(r.cpu1)})",
      },
      "\n");
  EXPECT_OK(ParseQuery(single_col_agg));
  std::string multi_output_col_agg =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)})"},
                    "\n");
  EXPECT_OK(ParseQuery(multi_output_col_agg));
  std::string multi_input_col_agg = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), "
       "'cpu2_mean' : pl.mean(r.cpu2)})"},
      "\n");
  EXPECT_OK(ParseQuery(multi_input_col_agg));
}

TEST(AggTest, not_allowed_by) {
  std::string single_col_bad_by_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : r, fn=lambda r :  {'cpu_count' : "
          "pl.count(r.cpu0)})",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_by_fn).ok());
  std::string single_col_bad_by_attr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=pl.mean, fn={'cpu_count' : pl.count(r.cpu0)})",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_by_attr).ok());
}
TEST(AggTest, not_allowed_agg_fn) {
  std::string single_col_bad_agg_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=1+2)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_bad_agg_fn).ok());
  std::string single_col_dict_by_not_pl = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=notpl.count)",
      },
      "\n");
  EXPECT_FALSE(ParseQuery(single_col_dict_by_not_pl).ok());
  std::string single_col_dict_by_no_attr_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
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
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})",
                     "result = rangeDF.Result(name='mapped')"},
                    "\n");
  EXPECT_OK(ParseQuery(single_col_map_sub));
}
TEST(TimeValueTest, basic) {
  std::string time_add_test =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "rangeDF = queryDF.Map(fn=lambda r : {'time_add' : r.cpu0 + pl.second})",
                     "result = rangeDF.Result(name='mapped')"},
                    "\n");
  EXPECT_OK(ParseQuery(time_add_test));
  std::string bad_attribute_value =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "rangeDF = queryDF.Map(fn=lambda r : {'time_add' : r.cpu0 + pl.notsecond})",
                     "result = rangeDF.Result(name='mapped')"},
                    "\n");
  EXPECT_FALSE(ParseQuery(bad_attribute_value).ok());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
