#include <gtest/gtest.h>

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/filter_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/proto/test_proto.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using testing::_;
using types::Int64Value;
using udf::FunctionContext;

// TOOD(zasgar): refactor these into test udfs.
class EqUDF : public udf::ScalarUDF {
 public:
  types::BoolValue Exec(FunctionContext*, types::Int64Value v1, types::Int64Value v2) {
    return v1.val == v2.val;
  }
};

class StrEqUDF : public udf::ScalarUDF {
 public:
  types::BoolValue Exec(FunctionContext*, types::StringValue v1, types::StringValue v2) {
    return v1 == v2;
  }
};

class FilterNodeTest : public ::testing::Test {
 public:
  FilterNodeTest() {
    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
    uda_registry_ = std::make_unique<udf::UDARegistry>("test_registry");
    EXPECT_OK(udf_registry_->Register<EqUDF>("eq"));
    EXPECT_OK(udf_registry_->Register<StrEqUDF>("eq"));
    auto table_store = std::make_shared<TableStore>();

    exec_state_ =
        std::make_unique<ExecState>(udf_registry_.get(), uda_registry_.get(), table_store);
    EXPECT_OK(exec_state_->AddScalarUDF(
        0, "eq", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64})));
    EXPECT_OK(exec_state_->AddScalarUDF(
        1, "eq", std::vector<types::DataType>({types::DataType::STRING, types::DataType::STRING})));
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
};

TEST_F(FilterNodeTest, basic) {
  auto op_proto = carnotpb::testutils::CreateTestFilterTwoCols();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false)
                       .AddColumn<types::Int64Value>({1, 1, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .AddColumn<types::StringValue>({"ABC", "DEF", "HELLO", "WORLD"})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false)
                          .AddColumn<types::Int64Value>({1, 1})
                          .AddColumn<types::Int64Value>({1, 3})
                          .AddColumn<types::StringValue>({"ABC", "DEF"})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .AddColumn<types::StringValue>({"Hello", "world", "now"})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true)
                          .AddColumn<types::Int64Value>({1})
                          .AddColumn<types::Int64Value>({1})
                          .AddColumn<types::StringValue>({"Hello"})
                          .get())
      .Close();
}

// TODO(zasgar/michelle): For some reason this test is not working. Need to debug.
TEST_F(FilterNodeTest, string_pred) {
  auto op_proto = carnotpb::testutils::CreateTestFilterTwoColsString();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::STRING, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::STRING, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false)
                       .AddColumn<types::StringValue>({"A", "B", "A", "D"})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false)
                          .AddColumn<types::StringValue>({"A", "A"})
                          .AddColumn<types::Int64Value>({1, 6})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true)
                       .AddColumn<types::StringValue>({"C", "B", "A"})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true)
                          .AddColumn<types::StringValue>({"A"})
                          .AddColumn<types::Int64Value>({6})
                          .get())
      .Close();
}

TEST_F(FilterNodeTest, child_fail) {
  auto op_proto = carnotpb::testutils::CreateTestFilterTwoCols();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester.ConsumeNextShouldFail(RowBatchBuilder(input_rd, 4, false)
                                   .AddColumn<types::Int64Value>({1, 2, 3, 4})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .get(),
                               error::InvalidArgument("args"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
