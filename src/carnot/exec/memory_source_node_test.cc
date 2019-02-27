#include <arrow/array.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "src/common/error.h"

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/proto/test_proto.h"
#include "src/carnot/udf/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using testing::_;

class MemorySourceNodeTest : public ::testing::Test {
 public:
  MemorySourceNodeTest() {
    auto op_proto = carnotpb::testutils::CreateTestSource1PB();
    plan_node_ = plan::MemorySourceOperator::FromProto(op_proto, 1);

    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
    uda_registry_ = std::make_unique<udf::UDARegistry>("test_registry");
    auto table_store = std::make_shared<TableStore>();
    exec_state_ =
        std::make_unique<ExecState>(udf_registry_.get(), uda_registry_.get(), table_store);

    auto descriptor =
        std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::INT64});
    RowDescriptor rd = RowDescriptor(descriptor);

    auto col1 = std::make_shared<Column>(Column(udf::UDFDataType::BOOLEAN, "col1"));
    std::vector<udf::BoolValue> col1_in1 = {true, false, true};
    std::vector<udf::BoolValue> col1_in2 = {false, false};
    EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
    EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in2, arrow::default_memory_pool())));

    auto col2 = std::make_shared<Column>(Column(udf::UDFDataType::INT64, "col2"));
    std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
    std::vector<udf::Int64Value> col2_in2 = {5, 6};
    EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in1, arrow::default_memory_pool())));
    EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in2, arrow::default_memory_pool())));

    std::shared_ptr<Table> table = std::make_shared<Table>(rd);
    exec_state_->table_store()->AddTable("cpu", table);

    EXPECT_OK(table->AddColumn(col1));
    EXPECT_OK(table->AddColumn(col2));
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
};

TEST_F(MemorySourceNodeTest, basic) {
  RowDescriptor output_rd({udf::UDFDataType::FLOAT64});
  MemorySourceNode src;
  MockExecNode mock_child_;
  src.AddChild(&mock_child_);
  EXPECT_OK(src.Init(*plan_node_, output_rd, {}));
  EXPECT_OK(src.Prepare(exec_state_.get()));
  EXPECT_OK(src.Open(exec_state_.get()));

  auto check_result_batch1 = [&](ExecState* exec_state, const RowBatch& child_rb) {
    EXPECT_EQ(exec_state, exec_state_.get());
    EXPECT_EQ(child_rb.num_rows(), 3);
    EXPECT_EQ(child_rb.num_columns(), 1);
    EXPECT_EQ(child_rb.desc().type(0), udf::UDFDataType::INT64);
    auto output_col = child_rb.ColumnAt(0);
    auto casted = reinterpret_cast<arrow::Int64Array*>(output_col.get());
    EXPECT_EQ(1, casted->Value(0));
    EXPECT_EQ(2, casted->Value(1));
    EXPECT_EQ(3, casted->Value(2));
  };

  EXPECT_CALL(mock_child_, ConsumeNextImpl(_, _))
      .Times(1)
      .WillOnce(testing::DoAll(testing::Invoke(check_result_batch1), testing::Return(Status::OK())))
      .RetiresOnSaturation();

  EXPECT_OK(src.GenerateNext(exec_state_.get()));

  auto check_result_batch2 = [&](ExecState* exec_state, const RowBatch& child_rb) {
    EXPECT_EQ(exec_state, exec_state_.get());
    EXPECT_EQ(child_rb.num_rows(), 2);
    EXPECT_EQ(child_rb.num_columns(), 1);
    EXPECT_EQ(child_rb.desc().type(0), udf::UDFDataType::INT64);
    auto output_col = child_rb.ColumnAt(0);
    auto casted = reinterpret_cast<arrow::Int64Array*>(output_col.get());
    EXPECT_EQ(5, casted->Value(0));
    EXPECT_EQ(6, casted->Value(1));
  };

  EXPECT_CALL(mock_child_, ConsumeNextImpl(_, _))
      .Times(1)
      .WillOnce(
          testing::DoAll(testing::Invoke(check_result_batch2), testing::Return(Status::OK())));

  EXPECT_OK(src.GenerateNext(exec_state_.get()));
  EXPECT_OK(src.Close(exec_state_.get()));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
