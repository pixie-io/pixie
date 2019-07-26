#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "src/common/base/base.h"

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/limit_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using testing::_;
using types::Int64Value;
using udf::FunctionContext;

class LimitNodeTest : public ::testing::Test {
 public:
  LimitNodeTest() {
    auto op_proto = planpb::testutils::CreateTestLimit1PB();
    plan_node_ = plan::LimitOperator::FromProto(op_proto, 1);

    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
    uda_registry_ = std::make_unique<udf::UDARegistry>("test_registry");
    auto table_store = std::make_shared<TableStore>();
    auto row_batch_queue = std::make_shared<RowBatchQueue>();

    exec_state_ = std::make_unique<ExecState>(udf_registry_.get(), uda_registry_.get(), table_store,
                                              row_batch_queue);
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
};

TEST_F(LimitNodeTest, single_batch) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 12, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 10, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, single_batch_exact_boundary) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 10, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 10, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9})
                          .get())
      .Close();
  EXPECT_EQ(false, exec_state_->keep_running());
}

TEST_F(LimitNodeTest, limits_records_split) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 6, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, false, false)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 6, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 4, 6, 8, 10, 12})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 4, 6, 8})
                          .get(),
                      0)
      .Close();
}

TEST_F(LimitNodeTest, limits_exact_boundary) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 6, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, false, false)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                          .get(),
                      0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 4, 6, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 4, 6, 8})
                          .get())
      .Close();

  EXPECT_EQ(false, exec_state_->keep_running());
}

TEST_F(LimitNodeTest, child_fail) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester.ConsumeNextShouldFail(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                                   .AddColumn<types::Int64Value>({1, 2, 3, 4})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .get(),
                               0, error::InvalidArgument("args"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
