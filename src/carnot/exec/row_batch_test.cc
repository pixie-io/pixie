#include <arrow/array.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/row_batch.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf.h"
#include "src/common/types/types.pb.h"

namespace pl {
namespace carnot {
namespace exec {

TEST(RowDescriptorTest, basic_test) {
  auto descriptor = std::vector<udf::UDFDataType>(
      {udf::UDFDataType::BOOLEAN, udf::UDFDataType::INT64, udf::UDFDataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);
  EXPECT_EQ(rd.type(0), udf::UDFDataType::BOOLEAN);
  EXPECT_EQ(rd.type(1), udf::UDFDataType::INT64);
  EXPECT_EQ(rd.type(2), udf::UDFDataType::FLOAT64);
}

class RowBatchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto descriptor = std::vector<udf::UDFDataType>(
        {types::DataType::BOOLEAN, types::DataType::INT64, types::DataType::FLOAT64});

    RowDescriptor rd = RowDescriptor(descriptor);
    rb_ = std::make_unique<RowBatch>(rd, 3);
  }
  std::unique_ptr<RowBatch> rb_;
};

TEST_F(RowBatchTest, basic_test) {
  std::vector<udf::BoolValue> in1 = {true, false, true};
  std::vector<udf::Int64Value> in2 = {3, 4, 5};
  std::vector<udf::Float64Value> in3 = {3.3, 4.1, 5.6};

  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in3, arrow::default_memory_pool())).ok());

  EXPECT_TRUE(rb_->HasColumn(0));
  EXPECT_TRUE(rb_->HasColumn(1));
  EXPECT_TRUE(rb_->HasColumn(2));
  EXPECT_FALSE(rb_->HasColumn(3));

  EXPECT_EQ(3, rb_->num_rows());
  EXPECT_EQ(3, rb_->num_columns());
  EXPECT_EQ(
      "RowBatch:\n  [\n  true,\n  false,\n  true\n]\n  [\n  3,\n  4,\n  5\n]\n  [\n  3.3,\n  "
      "4.1,\n  5.6\n]\n",
      rb_->DebugString());
}

TEST_F(RowBatchTest, extra_col_test) {
  std::vector<udf::BoolValue> in1 = {true, false, true};
  std::vector<udf::Int64Value> in2 = {3, 4, 5};
  std::vector<udf::Float64Value> in3 = {3.3, 4.1, 5.6};
  std::vector<udf::Float64Value> in4 = {3.3, 4.1, 5.6, 1.2};

  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in3, arrow::default_memory_pool())).ok());
  EXPECT_FALSE(rb_->AddColumn(udf::ToArrow(in4, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, extra_row_test) {
  std::vector<udf::BoolValue> in1 = {false, true, true, true};
  EXPECT_FALSE(rb_->AddColumn(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, incorrect_type_test) {
  std::vector<udf::Float64Value> in1 = {1.4, 1.2, 1.1};
  EXPECT_FALSE(rb_->AddColumn(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, num_bytes) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::INT64,
                                     types::DataType::FLOAT64, types::DataType::STRING});

  RowDescriptor rd = RowDescriptor(descriptor);
  auto rb_ = std::make_unique<RowBatch>(rd, 3);

  std::vector<udf::BoolValue> in1 = {true, false, true};
  std::vector<udf::Int64Value> in2 = {3, 4, 5};
  std::vector<udf::Float64Value> in3 = {3.3, 4.1, 5.6};
  std::vector<udf::StringValue> in4 = {"hello", "thisIs", "aString"};

  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in3, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(udf::ToArrow(in4, arrow::default_memory_pool())).ok());

  auto expected_bytes =
      3 * sizeof(bool) + 3 * sizeof(int64_t) + 3 * sizeof(double) + sizeof(char) * 18;
  EXPECT_EQ(expected_bytes, rb_->NumBytes());
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
