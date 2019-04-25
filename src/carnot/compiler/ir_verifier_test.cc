#include <gtest/gtest.h>

#include <memory>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ir_verifier.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace compiler {

TEST(ASTVisitor, compilation_test) {
  std::string from_expr = "From(table='cpu', select=['cpu0', 'cpu1']).Result(name='cpu2')";
  auto ir_graph_status = ParseQuery(from_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
  // check the connection of ig

  std::string from_range_expr =
      "From(table='cpu', select=['cpu0']).Range(start=0,stop=10).Result(name='cpu2')";
  ir_graph_status = ParseQuery(from_range_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(ASTVisitor, assign_functionality) {
  std::string simple_assign =
      "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Result(name='cpu2')";
  auto ir_graph_status = ParseQuery(simple_assign);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string assign_and_use =
      absl::StrJoin({"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
                     "queryDF.Range(start=0, stop=10).Result(name='cpu2')"},
                    "\n");
  ir_graph_status = ParseQuery(assign_and_use);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

// Range can only be after From, not after any other ops.
TEST(RangeTest, order_test) {
  std::string range_order_fail_map =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
                     "mapDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
                     "rangeDF = mapDF.Range(start=0,stop=10).Result(name='cpu2')"},
                    "\n");
  auto ir_graph_status = ParseQuery(range_order_fail_map);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_NOT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string range_order_fail_agg = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
       "mapDF = queryDF.Agg(fn=lambda r : {'sum' : pl.mean(r.cpu0)}, by=lambda r: r.cpu0)",
       "rangeDF = mapDF.Range(start=0,stop=10).Result(name='cpu2')"},
      "\n");
  ir_graph_status = ParseQuery(range_order_fail_agg);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_NOT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

// Map Tests
TEST(MapTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(single_col_map_sum);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
  std::string single_col_div_map_query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : "
          "pl.div(r.cpu0,r.cpu1)}).Result(name='cpu2')",
      },
      "\n");
  ir_graph_status = ParseQuery(single_col_div_map_query);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(MapTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : "
          "r.cpu2}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(multi_col);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(MapTest, bin_op_test) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1}).Result(name='cpu2')",
      },
      "\n");
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1}).Result(name='cpu2')",
      },
      "\n");
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'product' : r.cpu0 * r.cpu1}).Result(name='cpu2')",
      },
      "\n");
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(single_col_map_sum);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  ir_graph_status = ParseQuery(single_col_map_sub);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  ir_graph_status = ParseQuery(single_col_map_product);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  ir_graph_status = ParseQuery(single_col_map_quotient);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(MapTest, nested_expr_map) {
  std::string nested_expr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1 + "
          "r.cpu2}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(nested_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string nested_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0 + r.cpu1, "
          "r.cpu2)}).Result(name='cpu2')",
      },
      "\n");
  ir_graph_status = ParseQuery(nested_fn);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(AggTest, single_col_agg) {
  std::string single_col_agg = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu1)}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(single_col_agg);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string multi_output_col_agg =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
                     "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu2')"},
                    "\n");
  ir_graph_status = ParseQuery(multi_output_col_agg);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string multi_input_col_agg = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0,stop=10)",
       "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), "
       "'cpu2_mean' : pl.mean(r.cpu2)}).Result(name='cpu2')"},
      "\n");
  ir_graph_status = ParseQuery(multi_input_col_agg);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}
TEST(AggTest, not_allowed_by) {
  std::string single_col_bad_by_fn_expr = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : 1+2, fn=lambda r: {'cpu_count' : "
          "pl.count(r.cpu0)}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(single_col_bad_by_fn_expr);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_NOT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string single_col_dict_by_fn = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : {'cpu' : r.cpu0}, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu0)}).Result(name='cpu2')",
      },
      "\n");
  ir_graph_status = ParseQuery(single_col_dict_by_fn);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_NOT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
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
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_NOT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string add_combination = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0, stop=10)",
          "rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
          "pl.mean(r.cpu0)+2}).Result(name='cpu2')",
      },
      "\n");
  ir_graph_status = ParseQuery(add_combination);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  verifier = IRVerifier();
  status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_NOT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(TimeTest, basic) {
  std::string add_test = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=10)",
          "rangeDF = queryDF.Map(fn=lambda r : {'time' : r.cpu + pl.second}).Result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(add_test);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

TEST(RangeValueTests, now_stop) {
  std::string plc_now_test = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(start=0,stop=plc.now())",
       "rangeDF = queryDF.Map(fn=lambda r : {'plc_now' : r.cpu0 + pl.second})",
       "result = rangeDF.Result(name='mapped')"},
      "\n");
  auto ir_graph_status = ParseQuery(plc_now_test);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));
}

class VerifierTest : public ::testing::Test {
 protected:
  void SetUp() override { verifier_ = IRVerifier(); }
  /** @brief Convenient function to parse and save the result of the parse to the object. */
  Status ParseQueryTest(std::string query) {
    auto ir_graph_status = ParseQuery(query);
    if (ir_graph_status.ok()) {
      ir_graph_ = ir_graph_status.ValueOrDie();
    }
    return ir_graph_status.status();
  }

  Status VerifyGraphTest() {
    Status s = verifier_.VerifyGraphConnections(*ir_graph_);
    VLOG(2) << s.ToString();
    return s;
  }

  Status VerifyLineColTest() {
    Status s = verifier_.VerifyLineColGraph(*ir_graph_);
    VLOG(2) << s.ToString();
    return s;
  }

  std::shared_ptr<IR> ir_graph_;
  IRVerifier verifier_;
};

TEST_F(VerifierTest, filter_valid_query) {
  std::string query = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                     "'cpu1']).Filter(fn=lambda r : r.cpu0 >  0.5)",
                                     "queryDF.Result(name='filtered')"},
                                    "\n");
  ASSERT_OK(ParseQueryTest(query));
  EXPECT_OK(VerifyGraphTest());
  EXPECT_OK(VerifyLineColTest());
}

TEST_F(VerifierTest, filter_invalid_queries) {
  std::string int_val = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                       "'cpu1']).Filter(fn=1)",
                                       "queryDF.Result(name='filtered')"},
                                      "\n");
  ASSERT_OK(ParseQueryTest(int_val));
  EXPECT_NOT_OK(VerifyGraphTest());
}

TEST_F(VerifierTest, limit_valid_query) {
  std::string no_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                      "'cpu1']).Limit(rows=100)",
                                      "queryDF.Result(name='limited')"},
                                     "\n");
  // No arg shouldn't work.
  ASSERT_OK(ParseQueryTest(no_arg));
  EXPECT_OK(VerifyGraphTest());
  EXPECT_OK(VerifyLineColTest());
}

TEST_F(VerifierTest, limit_invalid_queries) {
  std::string no_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                      "'cpu1']).Limit()",
                                      "queryDF.Result(name='limited')"},
                                     "\n");
  // No arg shouldn't work.
  EXPECT_NOT_OK(ParseQueryTest(no_arg));

  std::string string_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                          "'cpu1']).Limit(rows='arg')",
                                          "queryDF.Result(name='limited')"},
                                         "\n");
  // String as an arg should not work.
  ASSERT_OK(ParseQueryTest(string_arg));
  EXPECT_NOT_OK(VerifyGraphTest());

  std::string float_arg = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                         "'cpu1']).Limit(rows=1.2)",
                                         "queryDF.Result(name='limited')"},
                                        "\n");
  // float as an arg should not work.
  ASSERT_OK(ParseQueryTest(float_arg));
  EXPECT_NOT_OK(VerifyGraphTest());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
