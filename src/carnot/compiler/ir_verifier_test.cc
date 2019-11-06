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
  std::string from_expr = "dataframe(table='cpu', select=['cpu0', 'cpu1']).result(name='cpu2')";
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
      "dataframe(table='cpu', select=['cpu0']).range(start=0,stop=10).result(name='cpu2')";
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
      "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).result(name='cpu2')";
  auto ir_graph_status = ParseQuery(simple_assign);
  VLOG(1) << ir_graph_status.ToString();
  ASSERT_OK(ir_graph_status);
  auto verifier = IRVerifier();
  auto status_connection = verifier.VerifyGraphConnections(*ir_graph_status.ValueOrDie());
  VLOG(1) << status_connection.ToString();
  EXPECT_OK(status_connection);
  EXPECT_OK(verifier.VerifyLineColGraph(*ir_graph_status.ValueOrDie()));

  std::string assign_and_use =
      absl::StrJoin({"queryDF = dataframe(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
                     "queryDF.range(start=0, stop=10).result(name='cpu2')"},
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

// Map Tests
TEST(MapTest, single_col_map) {
  std::string single_col_map_sum = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1}).result(name='cpu2')",
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
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sum' : "
          "pl.div(r.cpu0,r.cpu1)}).result(name='cpu2')",
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
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : "
          "r.cpu2}).result(name='cpu2')",
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
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1}).result(name='cpu2')",
      },
      "\n");
  std::string single_col_map_sub = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1}).result(name='cpu2')",
      },
      "\n");
  std::string single_col_map_product = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'product' : r.cpu0 * r.cpu1}).result(name='cpu2')",
      },
      "\n");
  std::string single_col_map_quotient = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1}).result(name='cpu2')",
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
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1 + "
          "r.cpu2}).result(name='cpu2')",
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
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'sum' : pl.div(r.cpu0 + r.cpu1, "
          "r.cpu2)}).result(name='cpu2')",
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
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
          "pl.count(r.cpu1)}).result(name='cpu2')",
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

  std::string multi_output_col_agg = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
       "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).result(name='cpu2')"},
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
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).range(start=0,stop=10)",
       "rangeDF = queryDF.agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), "
       "'cpu2_mean' : pl.mean(r.cpu2)}).result(name='cpu2')"},
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

TEST(TimeTest, basic) {
  std::string add_test = absl::StrJoin(
      {
          "queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=10)",
          "rangeDF = queryDF.map(fn=lambda r : {'time' : r.cpu + pl.second}).result(name='cpu2')",
      },
      "\n");
  auto ir_graph_status = ParseQuery(add_test);
  VLOG(1) << ir_graph_status.ToString();
  EXPECT_NOT_OK(ir_graph_status);
}

TEST(RangeValueTests, now_stop) {
  std::string plc_now_test = absl::StrJoin(
      {"queryDF = dataframe(table='cpu', select=['cpu0', 'cpu1']).range(start=0,stop=plc.now())",
       "rangeDF = queryDF.map(fn=lambda r : {'plc_now' : r.cpu0 })",
       "result = rangeDF.result(name='mapped')"},
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
  std::string query = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
                                     "'cpu1']).filter(fn=lambda r : r.cpu0 >  0.5)",
                                     "queryDF.result(name='filtered')"},
                                    "\n");
  ASSERT_OK(ParseQueryTest(query));
  EXPECT_OK(VerifyGraphTest());
  EXPECT_OK(VerifyLineColTest());
}

// TEST_F(VerifierTest, limit_valid_query) {
//   std::string no_arg = absl::StrJoin({"queryDF = dataframe(table='cpu', select=['cpu0', "
//                                       "'cpu1']).limit(rows=100)",
//                                       "queryDF.result(name='limited')"},
//                                      "\n");
//   // No arg shouldn't work.
//   ASSERT_OK(ParseQueryTest(no_arg));
//   EXPECT_OK(VerifyGraphTest());
//   EXPECT_OK(VerifyLineColTest());
// }

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
