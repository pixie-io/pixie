#include "src/carnot/compiler/ir_verifier.h"

#include <glog/logging.h>
#include <gtest/gtest.h>
#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/test_utils.h"
namespace pl {
namespace carnot {
namespace compiler {
// Checks whether we can actually compile into a graph.
void CheckStatusVector(const std::vector<Status>& status_vec, bool should_fail) {
  if (should_fail) {
    EXPECT_NE(status_vec.size(), 0);
  } else {
    EXPECT_EQ(status_vec.size(), 0);
  }
  for (const Status& s : status_vec) {
    VLOG(1) << s.msg();
  }
}

/**
 * @brief Verifies that the graph has the corrected connected components. Expects each query to be
 * properly compiled into the IRGraph, otherwise the entire test fails.
 *
 * @param query
 * @param should_fail
 */
void GraphVerify(const std::string& query, bool should_fail) {
  auto ir_graph_status = ParseQuery(query);
  auto verifier = IRVerifier();
  EXPECT_OK(ir_graph_status);
  // this only should run if the ir_graph is completed. Basically, ParseQuery should run
  // successfullly for it to actually verify properly.
  if (ir_graph_status.ok()) {
    auto ir_graph = ir_graph_status.ValueOrDie();
    CheckStatusVector(verifier.VerifyGraphConnections(ir_graph.get()), should_fail);
    // Line Col should be set no matter what - this is independent of whether the query is written
    // incorrectly or not.
    CheckStatusVector(verifier.VerifyLineColGraph(ir_graph.get()), false);
  }
}

TEST(ASTVisitor, compilation_test) {
  std::string from_expr = "From(table='cpu', select=['cpu0', 'cpu1'])";
  GraphVerify(from_expr, false);
  // check the connection of ig
  std::string from_range_expr = "From(table='cpu', select=['cpu0']).Range(time='-2m')";
  GraphVerify(from_range_expr, false);
}

TEST(ASTVisitor, assign_functionality) {
  std::string simple_assign = "queryDF = From(table='cpu', select=['cpu0', 'cpu1'])";
  GraphVerify(simple_assign, false);
  std::string assign_and_use = absl::StrJoin(
      {"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ])", "queryDF.Range(time = '-2m')"},
      "\n");
  GraphVerify(assign_and_use, false);
}

// Range can only be after From, not after any other ops.
TEST(RangeTest, order_test) {
  std::string range_order_fail_map =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
                     "mapDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
                     "rangeDF = mapDF.Range(time='-2m')"},
                    "\n");
  GraphVerify(range_order_fail_map, true);
  std::string range_order_fail_agg = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
       "mapDF = queryDF.Agg(fn=lambda r : {'sum' : pl.mean(r.cpu0)}, by=lambda r: r.cpu0)",
       "rangeDF = mapDF.Range(time='-2m')"},
      "\n");
  GraphVerify(range_order_fail_agg, true);
}

// Map Tests
TEST(MapTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
      },
      "\n");
  GraphVerify(single_col_map_sum, false);
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0,r.cpu1)})",
      },
      "\n");
  GraphVerify(single_col_div_map_query, false);
}

TEST(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : r.cpu2})",
      },
      "\n");
  GraphVerify(multi_col, false);
}

TEST(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
      },
      "\n");
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})",
      },
      "\n");
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'product' : r.cpu0 * r.cpu1})",
      },
      "\n");
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1})",
      },
      "\n");
  GraphVerify(single_col_map_sum, false);
  GraphVerify(single_col_map_sub, false);
  GraphVerify(single_col_map_product, false);
  GraphVerify(single_col_map_quotient, false);
}

TEST(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1 + r.cpu2})",
      },
      "\n");
  GraphVerify(nested_expr, false);
  std::string nested_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0 + r.cpu1, r.cpu2)})",
      },
      "\n");
  GraphVerify(nested_fn, false);
}

TEST(AggTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu1)})",
      },
      "\n");
  GraphVerify(single_col_agg, false);
  std::string multi_output_col_agg =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)})"},
                    "\n");
  GraphVerify(multi_output_col_agg, false);
  std::string multi_input_col_agg = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), "
       "'cpu2_mean' : pl.mean(r.cpu2)})"},
      "\n");
  GraphVerify(multi_input_col_agg, false);
}
TEST(AggTest, not_allowed_by) {
  std::string single_col_bad_by_fn_expr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : 1+2, fn=lambda r: {'cpu_count' : pl.count(r.cpu0)})",
      },
      "\n");
  GraphVerify(single_col_bad_by_fn_expr, true);

  std::string single_col_dict_by_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "rangeDF = queryDF.Agg(by=lambda r : {'cpu' : r.cpu0}, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu0)})",
      },
      "\n");
  GraphVerify(single_col_dict_by_fn, true);
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
