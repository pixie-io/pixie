#include "src/carnot/exec/memory_sink_node.h"

#include <arrow/array.h>
#include <arrow/memory_pool.h>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;

class MemorySinkNodeTest : public ::testing::Test {
 public:
  MemorySinkNodeTest() {
    auto op_proto = planpb::testutils::CreateTestSink2PB();
    plan_node_ = plan::MemorySinkOperator::FromProto(op_proto, 1);

    func_registry_ = std::make_unique<udf::Registry>("test_registry");

    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, sole::uuid4(), nullptr);
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(MemorySinkNodeTest, basic) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::BOOLEAN});
  RowDescriptor output_rd({});

  std::vector<types::Int64Value> col1_rb1 = {1, 2};
  std::vector<types::BoolValue> col2_rb1 = {true, false};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());

  std::vector<types::Int64Value> col1_rb2 = {3, 4};
  std::vector<types::BoolValue> col2_rb2 = {false, true};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());

  auto tester = exec::ExecNodeTester<MemorySinkNode, plan::MemorySinkOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());

  tester.ConsumeNext(RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                         .AddColumn<types::Int64Value>({1, 2})
                         .AddColumn<types::BoolValue>({true, false})
                         .get(),
                     false, 0);

  EXPECT_EQ(1, exec_state_->table_store()->GetTable("cpu_15s")->NumBatches());
  EXPECT_EQ(types::DataType::INT64,
            exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->data_type());
  EXPECT_EQ(types::DataType::BOOLEAN,
            exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->data_type());

  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->batch(0)->Equals(
      col1_rb1_arrow));
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->batch(0)->Equals(
      col2_rb1_arrow));

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({3, 4})
                       .AddColumn<types::BoolValue>({false, true})
                       .get(),
                   false, 0)
      .Close();

  EXPECT_EQ(2, exec_state_->table_store()->GetTable("cpu_15s")->NumBatches());
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->batch(1)->Equals(
      col1_rb2_arrow));
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->batch(1)->Equals(
      col2_rb2_arrow));
}

TEST_F(MemorySinkNodeTest, zero_row_row_batch_not_eos) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::BOOLEAN});
  RowDescriptor output_rd({});

  std::vector<types::Int64Value> col1_rb1 = {1, 2};
  std::vector<types::BoolValue> col2_rb1 = {true, false};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());

  std::vector<types::Int64Value> col1_rb2 = {3, 4};
  std::vector<types::BoolValue> col2_rb2 = {false, true};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());

  auto tester = exec::ExecNodeTester<MemorySinkNode, plan::MemorySinkOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());

  tester.ConsumeNext(RowBatchBuilder(input_rd, 0, /*eow*/ false, /*eos*/ false)
                         .AddColumn<types::Int64Value>({})
                         .AddColumn<types::BoolValue>({})
                         .get(),
                     false, 0);

  // Tests that a 0-row rb doesn't get written to the output table
  EXPECT_EQ(0, exec_state_->table_store()->GetTable("cpu_15s")->NumBatches());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({3, 4})
                       .AddColumn<types::BoolValue>({false, true})
                       .get(),
                   false, 0)
      .Close();

  EXPECT_EQ(1, exec_state_->table_store()->GetTable("cpu_15s")->NumBatches());
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(0)->batch(0)->Equals(
      col1_rb2_arrow));
  EXPECT_TRUE(exec_state_->table_store()->GetTable("cpu_15s")->GetColumn(1)->batch(0)->Equals(
      col2_rb2_arrow));
}

TEST_F(MemorySinkNodeTest, zero_row_row_batch_eos) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::BOOLEAN});
  RowDescriptor output_rd({});

  std::vector<types::Int64Value> col1_rb1 = {1, 2};
  std::vector<types::BoolValue> col2_rb1 = {true, false};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());

  std::vector<types::Int64Value> col1_rb2 = {3, 4};
  std::vector<types::BoolValue> col2_rb2 = {false, true};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());

  auto tester = exec::ExecNodeTester<MemorySinkNode, plan::MemorySinkOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());

  tester.ConsumeNext(RowBatchBuilder(input_rd, 0, /*eow*/ true, /*eos*/ true)
                         .AddColumn<types::Int64Value>({})
                         .AddColumn<types::BoolValue>({})
                         .get(),
                     false, 0);

  // Tests that a 0-row rb does get written to the output table
  EXPECT_EQ(1, exec_state_->table_store()->GetTable("cpu_15s")->NumBatches());
  auto rb_or_s = exec_state_->table_store()->GetTable("cpu_15s")->GetRowBatch(
      0, std::vector<int64_t>{0, 1}, arrow::default_memory_pool());
  EXPECT_OK(rb_or_s);
  auto rb = rb_or_s.ConsumeValueOrDie();
  EXPECT_EQ(0, rb->num_rows());
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
