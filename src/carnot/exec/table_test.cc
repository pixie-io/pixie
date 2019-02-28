#include <arrow/array.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/table.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace carnot {
namespace exec {

TEST(ColumnTest, basic_test) {
  auto col = Column(udf::UDFDataType::INT64, "col");
  EXPECT_EQ(col.data_type(), udf::UDFDataType::INT64);
  EXPECT_EQ(col.numBatches(), 0);

  std::vector<udf::Int64Value> in1 = {1, 2, 3};
  std::vector<udf::Int64Value> in2 = {3, 4};

  EXPECT_OK(col.AddBatch(udf::ToArrow(in1, arrow::default_memory_pool())));
  EXPECT_OK(col.AddBatch(udf::ToArrow(in2, arrow::default_memory_pool())));

  EXPECT_EQ(col.numBatches(), 2);
}

TEST(ColumnTest, wrong_chunk_type_test) {
  auto col = Column(udf::UDFDataType::INT64, "col");

  std::vector<udf::BoolValue> in1 = {true, false, true};

  EXPECT_FALSE(col.AddBatch(udf::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_EQ(col.numBatches(), 0);
}

TEST(TableTest, basic_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN, "col1");
  std::vector<udf::BoolValue> col1_in1 = {true, false, true};
  std::vector<udf::BoolValue> col1_in2 = {false, false};
  EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in2, arrow::default_memory_pool())));

  auto col2 = std::make_shared<Column>(udf::UDFDataType::INT64, "col2");
  std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<udf::Int64Value> col2_in2 = {5, 6};
  EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in2, arrow::default_memory_pool())));

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_OK(table.AddColumn(col2));
  EXPECT_EQ(table.NumBatches(), 2);

  auto rb1 = table.GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(udf::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 = table.GetRowBatch(1, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(udf::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(udf::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, wrong_schema_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(udf::UDFDataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, wrong_batch_size_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN, "col1");
  std::vector<udf::BoolValue> col1_in1 = {true, false, true};
  std::vector<udf::BoolValue> col1_in2 = {false, false};
  EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_in2, arrow::default_memory_pool())));
  auto col2 = std::make_shared<Column>(Column(udf::UDFDataType::INT64, "col2"));
  std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<udf::Int64Value> col2_in2 = {5, 6, 7};
  EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_in2, arrow::default_memory_pool())));

  EXPECT_TRUE(table.AddColumn(col1).ok());
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, wrong_col_number_test) {
  auto descriptor = std::vector<udf::UDFDataType>({types::DataType::BOOLEAN});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(udf::UDFDataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, write_row_batch) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(udf::UDFDataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_OK(table.AddColumn(col2));

  auto rb1 = RowBatch(rd, 2);
  std::vector<udf::BoolValue> col1_rb1 = {true, false};
  std::vector<udf::Int64Value> col2_rb1 = {1, 2};
  auto col1_rb1_arrow = udf::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = udf::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.NumBatches(), 1);

  EXPECT_TRUE(table.GetColumn(0)->batch(0)->Equals(col1_rb1_arrow));
  EXPECT_TRUE(table.GetColumn(1)->batch(0)->Equals(col2_rb1_arrow));
}

TEST(TableTest, hot_columns_test) {
  auto descriptor =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(udf::UDFDataType::BOOLEAN, "col1");
  std::vector<udf::BoolValue> col1_in1 = {true, false, true};
  auto col1_in1_wrapper =
      udf::ColumnWrapper::FromArrow(udf::ToArrow(col1_in1, arrow::default_memory_pool()));
  std::vector<udf::BoolValue> col1_in2 = {false, false};
  auto col1_in2_wrapper =
      udf::ColumnWrapper::FromArrow(udf::ToArrow(col1_in2, arrow::default_memory_pool()));

  auto col2 = std::make_shared<Column>(udf::UDFDataType::INT64, "col2");
  std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
  auto col2_in1_wrapper =
      udf::ColumnWrapper::FromArrow(udf::ToArrow(col2_in1, arrow::default_memory_pool()));
  std::vector<udf::Int64Value> col2_in2 = {5, 6};
  auto col2_in2_wrapper =
      udf::ColumnWrapper::FromArrow(udf::ToArrow(col2_in2, arrow::default_memory_pool()));

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_OK(table.AddColumn(col2));

  auto rb_wrapper_1 = std::make_unique<pl::stirling::ColumnWrapperRecordBatch>();
  rb_wrapper_1->push_back(col1_in1_wrapper);
  rb_wrapper_1->push_back(col2_in1_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_1)));

  auto rb_wrapper_2 = std::make_unique<pl::stirling::ColumnWrapperRecordBatch>();
  rb_wrapper_2->push_back(col1_in2_wrapper);
  rb_wrapper_2->push_back(col2_in2_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_2)));

  auto rb1 = table.GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(udf::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 = table.GetRowBatch(1, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(udf::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(udf::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, arrow_batches_test) {
  auto table = CarnotTestUtils::TestTable();

  auto record_batches_status = table->GetTableAsRecordBatches();
  ASSERT_TRUE(record_batches_status.ok());
  auto record_batches = record_batches_status.ConsumeValueOrDie();

  auto record_batch = record_batches[0];
  std::vector<udf::Float64Value> col1_exp1 = {0.5, 1.2, 5.3};
  std::vector<udf::Int64Value> col2_exp1 = {1, 2, 3};
  auto col1_batch = record_batch->column(0);
  auto col2_batch = record_batch->column(1);
  EXPECT_TRUE(col1_batch->Equals(udf::ToArrow(col1_exp1, arrow::default_memory_pool())));
  EXPECT_TRUE(col2_batch->Equals(udf::ToArrow(col2_exp1, arrow::default_memory_pool())));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
