#include <arrow/array.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/table.h"
#include "src/carnot/udf/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

TEST(ColumnTest, basic_test) {
  auto col = Column(udf::UDFDataType::INT64);
  EXPECT_EQ(col.data_type(), udf::UDFDataType::INT64);
  EXPECT_EQ(col.numChunks(), 0);

  std::vector<udf::Int64Value> in1 = {1, 2, 3};
  std::vector<udf::Int64Value> in2 = {3, 4};

  EXPECT_TRUE(col.AddChunk(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(col.AddChunk(udf::ToArrow(in2, arrow::default_memory_pool())).ok());

  EXPECT_EQ(col.numChunks(), 2);
}

TEST(ColumnTest, wrong_chunk_type_test) {
  auto col = Column(udf::UDFDataType::INT64);

  std::vector<udf::BoolValue> in1 = {true, false, true};

  EXPECT_FALSE(col.AddChunk(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_EQ(col.numChunks(), 0);
}

TEST(TableTest, basic_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(Column(udf::UDFDataType::BOOLEAN));
  std::vector<udf::BoolValue> col1_in1 = {true, false, true};
  std::vector<udf::BoolValue> col1_in2 = {false, false};
  EXPECT_TRUE(col1->AddChunk(udf::ToArrow(col1_in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(col1->AddChunk(udf::ToArrow(col1_in2, arrow::default_memory_pool())).ok());

  auto col2 = std::make_shared<Column>(Column(udf::UDFDataType::INT64));
  std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<udf::Int64Value> col2_in2 = {5, 6};
  EXPECT_TRUE(col2->AddChunk(udf::ToArrow(col2_in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(col2->AddChunk(udf::ToArrow(col2_in2, arrow::default_memory_pool())).ok());

  EXPECT_TRUE(table.AddColumn(col1).ok());
  EXPECT_TRUE(table.AddColumn(col2).ok());
  EXPECT_EQ(table.numBatches(), 2);

  auto rb1 = table.GetRowBatch(0, std::vector<int64_t>({0, 1})).ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(udf::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 = table.GetRowBatch(1, std::vector<int64_t>({0, 1})).ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(udf::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(udf::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, wrong_schema_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(Column(udf::UDFDataType::BOOLEAN));
  auto col2 = std::make_shared<Column>(Column(udf::UDFDataType::INT64));

  EXPECT_TRUE(table.AddColumn(col1).ok());
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, wrong_batch_size_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(Column(udf::UDFDataType::BOOLEAN));
  std::vector<udf::BoolValue> col1_in1 = {true, false, true};
  std::vector<udf::BoolValue> col1_in2 = {false, false};
  EXPECT_TRUE(col1->AddChunk(udf::ToArrow(col1_in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(col1->AddChunk(udf::ToArrow(col1_in2, arrow::default_memory_pool())).ok());
  auto col2 = std::make_shared<Column>(Column(udf::UDFDataType::INT64));
  std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<udf::Int64Value> col2_in2 = {5, 6, 7};
  EXPECT_TRUE(col2->AddChunk(udf::ToArrow(col2_in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(col2->AddChunk(udf::ToArrow(col2_in2, arrow::default_memory_pool())).ok());

  EXPECT_TRUE(table.AddColumn(col1).ok());
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, wrong_col_number_test) {
  auto descriptor = std::vector<udf::UDFDataType>({types::DataType::BOOLEAN});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(Column(udf::UDFDataType::BOOLEAN));
  auto col2 = std::make_shared<Column>(Column(udf::UDFDataType::INT64));

  EXPECT_TRUE(table.AddColumn(col1).ok());
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
