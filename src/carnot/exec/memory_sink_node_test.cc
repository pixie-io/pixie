#include <arrow/array.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "src/common/error.h"

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/proto/test_proto.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "third_party/arrow/cpp/src/arrow/array.h"

namespace pl {
namespace carnot {
namespace exec {

using testing::_;

class MemorySinkNodeTest : public ::testing::Test {
 public:
  MemorySinkNodeTest() {
    auto op_proto = carnotpb::testutils::CreateTestSink2PB();
    plan_node_ = plan::MemorySinkOperator::FromProto(op_proto, 1);

    auto udf_registry = std::make_shared<udf::ScalarUDFRegistry>("test_registry");
    auto uda_registry = std::make_shared<udf::UDARegistry>("test_registry");

    auto table_store = std::make_shared<TableStore>();
    exec_state_ = std::make_unique<ExecState>(udf_registry, uda_registry, table_store);
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
};

TEST_F(MemorySinkNodeTest, basic) {
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::BOOLEAN});

  auto rb1 = RowBatch(output_rd, 2);
  std::vector<udf::Int64Value> col1_rb1 = {1, 2};
  std::vector<udf::BoolValue> col2_rb1 = {true, false};
  auto col1_rb1_arrow = udf::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = udf::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  MemorySinkNode sink;
  EXPECT_OK(sink.Init(*plan_node_, output_rd, {}));
  EXPECT_OK(sink.Prepare(exec_state_.get()));
  EXPECT_OK(sink.Open(exec_state_.get()));

  EXPECT_OK(sink.ConsumeNext(exec_state_.get(), rb1));

  EXPECT_EQ(1, exec_state_->table_store()->GetTable("cpu_15s")->numBatches());
  EXPECT_EQ(types::DataType::INT64,
            exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->data_type());
  EXPECT_EQ(types::DataType::BOOLEAN,
            exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->data_type());

  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->chunk(0)->Equals(
      col1_rb1_arrow));
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->chunk(0)->Equals(
      col2_rb1_arrow));

  auto rb2 = RowBatch(output_rd, 2);
  std::vector<udf::Int64Value> col1_rb2 = {3, 4};
  std::vector<udf::BoolValue> col2_rb2 = {false, true};
  auto col1_rb2_arrow = udf::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb2_arrow = udf::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_TRUE(rb2.AddColumn(col1_rb2_arrow).ok());
  EXPECT_TRUE(rb2.AddColumn(col2_rb2_arrow).ok());

  EXPECT_OK(sink.ConsumeNext(exec_state_.get(), rb2));
  EXPECT_OK(sink.Close(exec_state_.get()));

  EXPECT_EQ(2, exec_state_->table_store()->GetTable("cpu_15s")->numBatches());
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->chunk(1)->Equals(
      col1_rb2_arrow));
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->chunk(1)->Equals(
      col2_rb2_arrow));
}
}  // namespace exec
}  // namespace carnot
}  // namespace pl
