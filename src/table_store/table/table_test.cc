#include <arrow/array.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/shared/types/arrow_adapter.h"
#include "src/table_store/proto/schema.pb.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/table/table.h"

namespace pl {
namespace table_store {

namespace {
// TOOD(zasgar): deduplicate this with exec/test_utils.
std::shared_ptr<Table> TestTable() {
  schema::Relation rel({types::DataType::FLOAT64, types::DataType::INT64}, {"col1", "col2"});
  auto table = std::make_shared<Table>(rel);

  auto col1 = table->GetColumn(0);
  std::vector<types::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
  std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
  PL_CHECK_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  PL_CHECK_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

  auto col2 = table->GetColumn(1);
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  PL_CHECK_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  PL_CHECK_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

  return table;
}

}  // namespace

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
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

  Table table(rel);
  auto col1 = table.GetColumn(0);
  auto col2 = table.GetColumn(1);

  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  std::vector<types::BoolValue> col1_in2 = {false, false};

  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(col2->AddBatch(types::ToArrow(col2_in2, arrow::default_memory_pool())));

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

TEST(TableTest, bytes_test) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  Table table(rel);

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.NumBatches(), 1);
  EXPECT_EQ(table.NumBytes(), rb1_size);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_EQ(table.NumBatches(), 2);
  EXPECT_EQ(table.NumBytes(), rb1_size + rb2_size);

  std::vector<types::Int64Value> time_hot_col1 = {1, 5, 3};
  std::vector<types::StringValue> time_hot_col2 = {"test", "abc", "de"};
  auto wrapper_batch_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1 = std::make_shared<types::Int64ValueColumnWrapper>(3);
  col_wrapper_1->Clear();
  for (const auto& num : time_hot_col1) {
    col_wrapper_1->Append(num);
  }
  auto col_wrapper_2 = std::make_shared<types::StringValueColumnWrapper>(3);
  col_wrapper_2->Clear();
  for (const auto& num : time_hot_col2) {
    col_wrapper_2->Append(num);
  }
  wrapper_batch_1->push_back(col_wrapper_1);
  wrapper_batch_1->push_back(col_wrapper_2);
  int64_t rb3_size = 3 * sizeof(int64_t) + 9 * sizeof(char);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));

  EXPECT_EQ(table.NumBatches(), 3);
  EXPECT_EQ(table.NumBytes(), rb1_size + rb2_size + rb3_size);
}

TEST(TableTest, wrong_batch_size_test) {
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::FLOAT64}, {"col1", "col2"});

  Table table(rel);
  auto col1 = table.GetColumn(0);
  auto col2 = table.GetColumn(1);

  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  std::vector<types::BoolValue> col1_in2 = {false, false};
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(col1->AddBatch(types::ToArrow(col1_in2, arrow::default_memory_pool())));

  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  EXPECT_NOT_OK(col2->AddBatch(types::ToArrow(col2_in1, arrow::default_memory_pool())));
}

TEST(TableTest, write_row_batch) {
  auto rd = schema::RowDescriptor({types::DataType::BOOLEAN, types::DataType::INT64});
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

  Table table(rel);

  schema::RowBatch rb1(rd, 2);
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
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

  Table table(rel);

  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  auto col1_in1_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col1_in1, arrow::default_memory_pool()));
  std::vector<types::BoolValue> col1_in2 = {false, false};
  auto col1_in2_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col1_in2, arrow::default_memory_pool()));

  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  auto col2_in1_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col2_in1, arrow::default_memory_pool()));
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  auto col2_in2_wrapper =
      types::ColumnWrapper::FromArrow(types::ToArrow(col2_in2, arrow::default_memory_pool()));

  auto rb_wrapper_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
  rb_wrapper_1->push_back(col1_in1_wrapper);
  rb_wrapper_1->push_back(col2_in1_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_1)));

  auto rb_wrapper_2 = std::make_unique<types::ColumnWrapperRecordBatch>();
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
  auto table = TestTable();

  auto record_batches_status = table->GetTableAsRecordBatches();
  ASSERT_OK(record_batches_status);
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
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});
  schema::RowDescriptor rd({types::DataType::BOOLEAN, types::DataType::INT64});

  Table table(rel);

  schema::RowBatch rb1(rd, 2);
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
  schema::Relation relation(std::vector<types::DataType>({types::DataType::TIME64NS}),
                            std::vector<std::string>({"time_"}));
  Table table(relation);
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
  auto wrapper_batch_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1 = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper_1->Clear();
  for (const auto& num : time_hot_col1) {
    col_wrapper_1->Append(num);
  }
  wrapper_batch_1->push_back(col_wrapper_1);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));
  auto wrapper_batch_2 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_2 = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper_2->Clear();
  for (const auto& num : time_hot_col2) {
    col_wrapper_2->Append(num);
  }
  wrapper_batch_2->push_back(col_wrapper_2);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_2)));
  auto wrapper_batch_3 = std::make_unique<types::ColumnWrapperRecordBatch>();
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

TEST(TableTest, ToProto) {
  auto table = TestTable();
  table_store::schemapb::Table table_proto;
  EXPECT_OK(table->ToProto(&table_proto));
  LOG(ERROR) << table_proto.DebugString();

  std::string expected = R"(
relation {
  columns {
    column_name: "col1"
    column_type: FLOAT64
  }
  columns {
    column_name: "col2"
    column_type: INT64
  }
}
row_batches {
  cols {
    float64_data {
      data: 0.5
      data: 1.2
      data: 5.3
    }
  }
  cols {
    int64_data {
      data: 1
      data: 2
      data: 3
    }
  }
  num_rows: 3
}
row_batches {
  cols {
    float64_data {
      data: 0.1
      data: 5.1
    }
  }
  cols {
    int64_data {
      data: 5
      data: 6
    }
  }
  num_rows: 2
})";

  google::protobuf::util::MessageDifferencer differ;
  table_store::schemapb::Table expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected, &expected_proto));
  EXPECT_TRUE(differ.Compare(expected_proto, table_proto));
}

}  // namespace table_store
}  // namespace pl
