#include <arrow/array.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/table.h"
#include "src/carnot/exec/test_utils.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace carnot {
namespace exec {

TEST(ColumnTest, basic_test) {
  auto col = Column(types::DataType::INT64, "col");
  EXPECT_EQ(col.data_type(), types::DataType::INT64);
  EXPECT_EQ(col.numBatches(), 0);

  std::vector<types::Int64Value> in1 = {1, 2, 3};
  std::vector<types::Int64Value> in2 = {3, 4};

  EXPECT_OK(col.AddBatch(types::ToArrow(in1, arrow::default_memory_pool())));
  EXPECT_OK(col.AddBatch(types::ToArrow(in2, arrow::default_memory_pool())));

  EXPECT_EQ(col.numBatches(), 2);
}

TEST(ColumnTest, wrong_chunk_type_test) {
  auto col = Column(types::DataType::INT64, "col");

  std::vector<types::BoolValue> in1 = {true, false, true};

  EXPECT_FALSE(col.AddBatch(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_EQ(col.numBatches(), 0);
}

TEST(TableTest, basic_test) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  std::vector<types::BoolValue> col1_in2 = {false, false};
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

  auto col2 = std::make_shared<Column>(types::DataType::INT64, "col2");
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_OK(table.AddColumn(col2));
  EXPECT_EQ(table.NumBatches(), 2);

  auto rb1 = table.GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb1_sliced =
      table.GetRowBatchSlice(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool(), 1, 3)
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1_sliced->ColumnAt(0)->Equals(
      types::ToArrow(std::vector<types::BoolValue>({false, true}), arrow::default_memory_pool())));
  EXPECT_TRUE(rb1_sliced->ColumnAt(1)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({2, 3}), arrow::default_memory_pool())));

  auto rb2 = table.GetRowBatch(1, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, wrong_schema_test) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(types::DataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, wrong_batch_size_test) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  std::vector<types::BoolValue> col1_in2 = {false, false};
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  auto col2 = std::make_shared<Column>(Column(types::DataType::INT64, "col2"));
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col2_in2 = {5, 6, 7};
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

  EXPECT_TRUE(table.AddColumn(col1).ok());
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, wrong_col_number_test) {
  auto descriptor = std::vector<types::DataType>({types::DataType::BOOLEAN});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(types::DataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_FALSE(table.AddColumn(col2).ok());
}

TEST(TableTest, write_row_batch) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(types::DataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_OK(table.AddColumn(col2));

  auto rb1 = RowBatch(rd, 2);
  std::vector<types::BoolValue> col1_rb1 = {true, false};
  std::vector<types::Int64Value> col2_rb1 = {1, 2};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.NumBatches(), 1);

  EXPECT_TRUE(table.GetColumn(0)->batch(0)->Equals(col1_rb1_arrow));
  EXPECT_TRUE(table.GetColumn(1)->batch(0)->Equals(col2_rb1_arrow));
}

TEST(TableTest, hot_batches_test) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  auto col1_in1_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col1_in1, arrow::default_memory_pool()));
  std::vector<types::BoolValue> col1_in2 = {false, false};
  auto col1_in2_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col1_in2, arrow::default_memory_pool()));

  auto col2 = std::make_shared<Column>(types::DataType::INT64, "col2");
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  auto col2_in1_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col2_in1, arrow::default_memory_pool()));
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  auto col2_in2_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col2_in2, arrow::default_memory_pool()));

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
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 = table.GetRowBatch(1, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
                 .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, arrow_batches_test) {
  auto table = CarnotTestUtils::TestTable();

  auto record_batches_status = table->GetTableAsRecordBatches();
  ASSERT_TRUE(record_batches_status.ok());
  auto record_batches = record_batches_status.ConsumeValueOrDie();

  auto record_batch = record_batches[0];
  std::vector<types::Float64Value> col1_exp1 = {0.5, 1.2, 5.3};
  std::vector<types::Int64Value> col2_exp1 = {1, 2, 3};
  auto col1_batch = record_batch->column(0);
  auto col2_batch = record_batch->column(1);
  EXPECT_TRUE(col1_batch->Equals(types::ToArrow(col1_exp1, arrow::default_memory_pool())));
  EXPECT_TRUE(col2_batch->Equals(types::ToArrow(col2_exp1, arrow::default_memory_pool())));
}

TEST(TableTest, greater_than_eq_eq) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  Table table = Table(rd);

  auto col1 = std::make_shared<Column>(types::DataType::BOOLEAN, "col1");
  auto col2 = std::make_shared<Column>(types::DataType::INT64, "col2");

  EXPECT_OK(table.AddColumn(col1));
  EXPECT_OK(table.AddColumn(col2));

  auto rb1 = RowBatch(rd, 2);
  std::vector<types::BoolValue> col1_rb1 = {true, false};
  std::vector<types::Int64Value> col2_rb1 = {1, 2};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.NumBatches(), 1);
}

TEST(TableTest, find_batch_position_greater_or_eq) {
  auto relation = plan::Relation(std::vector<types::DataType>({types::DataType::TIME64NS}),
                                 std::vector<std::string>({"time_"}));
  Table table = Table(relation);
  std::vector<types::Time64NSValue> time_cold_col1 = {2, 3, 4, 6};
  std::vector<types::Time64NSValue> time_cold_col2 = {8, 8, 8};
  std::vector<types::Time64NSValue> time_cold_col3 = {8, 9, 11};

  EXPECT_OK(
      table.GetColumn(0)->AddBatch(types::ToArrow(time_cold_col1, arrow::default_memory_pool())));
  EXPECT_OK(
      table.GetColumn(0)->AddBatch(types::ToArrow(time_cold_col2, arrow::default_memory_pool())));
  EXPECT_OK(
      table.GetColumn(0)->AddBatch(types::ToArrow(time_cold_col3, arrow::default_memory_pool())));

  std::vector<types::Time64NSValue> time_hot_col1 = {15, 16, 19};
  std::vector<types::Time64NSValue> time_hot_col2 = {21, 21, 21};
  std::vector<types::Time64NSValue> time_hot_col3 = {21, 23};
  auto wrapper_batch_1 = std::make_unique<pl::stirling::ColumnWrapperRecordBatch>();
  auto col_wrapper_1 = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper_1->Clear();
  for (const auto& num : time_hot_col1) {
    col_wrapper_1->Append(num);
  }
  wrapper_batch_1->push_back(col_wrapper_1);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));
  auto wrapper_batch_2 = std::make_unique<pl::stirling::ColumnWrapperRecordBatch>();
  auto col_wrapper_2 = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper_2->Clear();
  for (const auto& num : time_hot_col2) {
    col_wrapper_2->Append(num);
  }
  wrapper_batch_2->push_back(col_wrapper_2);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_2)));
  auto wrapper_batch_3 = std::make_unique<pl::stirling::ColumnWrapperRecordBatch>();
  auto col_wrapper_3 = std::make_shared<types::Time64NSValueColumnWrapper>(2);
  col_wrapper_3->Clear();
  for (const auto& num : time_hot_col3) {
    col_wrapper_3->Append(num);
  }
  wrapper_batch_3->push_back(col_wrapper_3);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_3)));

  auto batch_pos = table.FindBatchPositionGreaterThanOrEqual(0, arrow::default_memory_pool());
  EXPECT_EQ(0, batch_pos.batch_idx);
  EXPECT_EQ(0, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(5, arrow::default_memory_pool());
  EXPECT_EQ(0, batch_pos.batch_idx);
  EXPECT_EQ(3, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(6, arrow::default_memory_pool());
  EXPECT_EQ(0, batch_pos.batch_idx);
  EXPECT_EQ(3, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(8, arrow::default_memory_pool());
  EXPECT_EQ(1, batch_pos.batch_idx);
  EXPECT_EQ(0, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(10, arrow::default_memory_pool());
  EXPECT_EQ(2, batch_pos.batch_idx);
  EXPECT_EQ(2, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(13, arrow::default_memory_pool());
  EXPECT_EQ(3, batch_pos.batch_idx);
  EXPECT_EQ(0, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(21, arrow::default_memory_pool());
  EXPECT_EQ(4, batch_pos.batch_idx);
  EXPECT_EQ(0, batch_pos.row_idx);

  batch_pos = table.FindBatchPositionGreaterThanOrEqual(24, arrow::default_memory_pool());
  EXPECT_EQ(-1, batch_pos.batch_idx);
  EXPECT_EQ(-1, batch_pos.row_idx);
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
