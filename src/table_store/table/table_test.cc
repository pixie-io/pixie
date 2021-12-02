/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <absl/synchronization/notification.h>
#include <arrow/array.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <random>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schemapb/schema.pb.h"
#include "src/table_store/table/table.h"

namespace px {
namespace table_store {

namespace {
// TOOD(zasgar): deduplicate this with exec/test_utils.
std::shared_ptr<Table> TestTable() {
  schema::Relation rel({types::DataType::FLOAT64, types::DataType::INT64}, {"col1", "col2"});
  auto table = Table::Create("test_table", rel);

  auto rb1 = schema::RowBatch(schema::RowDescriptor(rel.col_types()), 3);
  std::vector<types::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  PL_CHECK_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  PL_CHECK_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  PL_CHECK_OK(table->WriteRowBatch(rb1));

  auto rb2 = schema::RowBatch(schema::RowDescriptor(rel.col_types()), 2);
  std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  PL_CHECK_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  PL_CHECK_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  PL_CHECK_OK(table->WriteRowBatch(rb2));

  return table;
}

}  // namespace

static inline BatchSlice BatchSliceFromRowIds(int64_t uniq_row_start_idx,
                                              int64_t uniq_row_end_idx) {
  return BatchSlice{false, -1, -1, -1, -1, uniq_row_start_idx, uniq_row_end_idx};
}

TEST(TableTest, basic_test) {
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

  std::shared_ptr<Table> table_ptr = Table::Create("test_table", rel);
  Table& table = *table_ptr;

  auto rb1 = schema::RowBatch(schema::RowDescriptor(rel.col_types()), 3);
  std::vector<types::BoolValue> col1_in1 = {true, false, true};
  std::vector<types::Int64Value> col2_in1 = {1, 2, 3};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(table.WriteRowBatch(rb1));

  auto rb2 = schema::RowBatch(schema::RowDescriptor(rel.col_types()), 2);
  std::vector<types::BoolValue> col1_in2 = {false, false};
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(table.WriteRowBatch(rb2));

  auto actual_rb1 = table
                        .GetRowBatchSlice(table.FirstBatch(), std::vector<int64_t>({0, 1}),
                                          arrow::default_memory_pool())
                        .ConsumeValueOrDie();
  EXPECT_TRUE(
      actual_rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(
      actual_rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto slice = BatchSliceFromRowIds(1, 2);
  auto rb1_sliced =
      table.GetRowBatchSlice(slice, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1_sliced->ColumnAt(0)->Equals(
      types::ToArrow(std::vector<types::BoolValue>({false, true}), arrow::default_memory_pool())));
  EXPECT_TRUE(rb1_sliced->ColumnAt(1)->Equals(
      types::ToArrow(std::vector<types::Int64Value>({2, 3}), arrow::default_memory_pool())));

  slice = BatchSliceFromRowIds(3, 4);
  auto actual_rb2 =
      table.GetRowBatchSlice(slice, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(
      actual_rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(
      actual_rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, bytes_test) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  std::shared_ptr<Table> table_ptr = Table::Create("test_table", rel);
  Table& table = *table_ptr;

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size);

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

  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size + rb3_size);
}

TEST(TableTest, bytes_test_w_compaction) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char);

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

  // Make minimum batch size rb1_size + rb2_size so that compaction causes 2 of the 3 batches to be
  // compacted into cold.
  std::shared_ptr<Table> table_ptr =
      std::make_shared<Table>("test_table", rel, 128 * 1024, rb1_size + rb2_size);
  Table& table = *table_ptr;

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));

  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));

  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size + rb3_size);
}

TEST(TableTest, expiry_test) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  Table table("test_table", rel, 60);

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size);

  schema::RowBatch rb3(rd, 2);
  std::vector<types::Int64Value> col1_rb3 = {4, 5};
  std::vector<types::StringValue> col2_rb3 = {"longerstring", "hellohellohello"};
  auto col1_rb3_arrow = types::ToArrow(col1_rb3, arrow::default_memory_pool());
  auto col2_rb3_arrow = types::ToArrow(col2_rb3, arrow::default_memory_pool());
  EXPECT_OK(rb3.AddColumn(col1_rb3_arrow));
  EXPECT_OK(rb3.AddColumn(col2_rb3_arrow));
  int64_t rb3_size = 2 * sizeof(int64_t) + 27 * sizeof(char);

  EXPECT_OK(table.WriteRowBatch(rb3));
  EXPECT_EQ(table.GetTableStats().bytes, rb3_size);

  std::vector<types::Int64Value> time_hot_col1 = {1};
  std::vector<types::StringValue> time_hot_col2 = {"a"};
  auto wrapper_batch_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1 = std::make_shared<types::Int64ValueColumnWrapper>(1);
  col_wrapper_1->Clear();
  for (const auto& num : time_hot_col1) {
    col_wrapper_1->Append(num);
  }
  auto col_wrapper_2 = std::make_shared<types::StringValueColumnWrapper>(1);
  col_wrapper_2->Clear();
  for (const auto& num : time_hot_col2) {
    col_wrapper_2->Append(num);
  }
  wrapper_batch_1->push_back(col_wrapper_1);
  wrapper_batch_1->push_back(col_wrapper_2);
  int64_t rb4_size = 1 * sizeof(int64_t) + 1 * sizeof(char);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));

  EXPECT_EQ(table.GetTableStats().bytes, rb3_size + rb4_size);

  std::vector<types::Int64Value> time_hot_col1_2 = {1, 2, 3, 4, 5};
  std::vector<types::StringValue> time_hot_col2_2 = {"abcdef", "ghi", "jklmno", "pqr", "tu"};
  auto wrapper_batch_1_2 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1_2 = std::make_shared<types::Int64ValueColumnWrapper>(5);
  col_wrapper_1_2->Clear();
  for (const auto& num : time_hot_col1_2) {
    col_wrapper_1_2->Append(num);
  }
  auto col_wrapper_2_2 = std::make_shared<types::StringValueColumnWrapper>(5);
  col_wrapper_2_2->Clear();
  for (const auto& num : time_hot_col2_2) {
    col_wrapper_2_2->Append(num);
  }
  wrapper_batch_1_2->push_back(col_wrapper_1_2);
  wrapper_batch_1_2->push_back(col_wrapper_2_2);
  int64_t rb5_size = 5 * sizeof(int64_t) + 20 * sizeof(char);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1_2)));

  EXPECT_EQ(table.GetTableStats().bytes, rb5_size);
}

TEST(TableTest, expiry_test_w_compaction) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char);

  schema::RowBatch rb3(rd, 2);
  std::vector<types::Int64Value> col1_rb3 = {4, 5};
  std::vector<types::StringValue> col2_rb3 = {"longerstring", "hellohellohello"};
  auto col1_rb3_arrow = types::ToArrow(col1_rb3, arrow::default_memory_pool());
  auto col2_rb3_arrow = types::ToArrow(col2_rb3, arrow::default_memory_pool());
  EXPECT_OK(rb3.AddColumn(col1_rb3_arrow));
  EXPECT_OK(rb3.AddColumn(col2_rb3_arrow));
  int64_t rb3_size = 2 * sizeof(int64_t) + 27 * sizeof(char);

  std::vector<types::Int64Value> time_hot_col1 = {1};
  std::vector<types::StringValue> time_hot_col2 = {"a"};
  auto wrapper_batch_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1 = std::make_shared<types::Int64ValueColumnWrapper>(1);
  col_wrapper_1->Clear();
  for (const auto& num : time_hot_col1) {
    col_wrapper_1->Append(num);
  }
  auto col_wrapper_2 = std::make_shared<types::StringValueColumnWrapper>(1);
  col_wrapper_2->Clear();
  for (const auto& num : time_hot_col2) {
    col_wrapper_2->Append(num);
  }
  wrapper_batch_1->push_back(col_wrapper_1);
  wrapper_batch_1->push_back(col_wrapper_2);
  int64_t rb4_size = 1 * sizeof(int64_t) + 1 * sizeof(char);

  std::vector<types::Int64Value> time_hot_col1_2 = {1, 2, 3, 4, 5};
  std::vector<types::StringValue> time_hot_col2_2 = {"abcdef", "ghi", "jklmno", "pqr", "tu"};
  auto wrapper_batch_1_2 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_1_2 = std::make_shared<types::Int64ValueColumnWrapper>(5);
  col_wrapper_1_2->Clear();
  for (const auto& num : time_hot_col1_2) {
    col_wrapper_1_2->Append(num);
  }
  auto col_wrapper_2_2 = std::make_shared<types::StringValueColumnWrapper>(5);
  col_wrapper_2_2->Clear();
  for (const auto& num : time_hot_col2_2) {
    col_wrapper_2_2->Append(num);
  }
  wrapper_batch_1_2->push_back(col_wrapper_1_2);
  wrapper_batch_1_2->push_back(col_wrapper_2_2);
  int64_t rb5_size = 5 * sizeof(int64_t) + 20 * sizeof(char);

  Table table("test_table", rel, 60, 40);
  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size);

  EXPECT_OK(table.WriteRowBatch(rb3));
  EXPECT_EQ(table.GetTableStats().bytes, rb3_size);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));
  EXPECT_EQ(table.GetTableStats().bytes, rb3_size + rb4_size);

  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1_2)));
  EXPECT_EQ(table.GetTableStats().bytes, rb5_size);
}

TEST(TableTest, batch_size_too_big) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  Table table("test_table", rel, 10);

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  EXPECT_NOT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, 0);
}

TEST(TableTest, write_row_batch) {
  auto rd = schema::RowDescriptor({types::DataType::BOOLEAN, types::DataType::INT64});
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

  std::shared_ptr<Table> table_ptr = Table::Create("test_table", rel);
  Table& table = *table_ptr;

  schema::RowBatch rb1(rd, 2);
  std::vector<types::BoolValue> col1_rb1 = {true, false};
  std::vector<types::Int64Value> col2_rb1 = {1, 2};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  EXPECT_OK(table.WriteRowBatch(rb1));

  auto rb_or_s = table.GetRowBatchSlice(table.FirstBatch(), {0, 1}, arrow::default_memory_pool());
  ASSERT_OK(rb_or_s);
  auto actual_rb = rb_or_s.ConsumeValueOrDie();
  EXPECT_TRUE(actual_rb->ColumnAt(0)->Equals(col1_rb1_arrow));
  EXPECT_TRUE(actual_rb->ColumnAt(1)->Equals(col2_rb1_arrow));
}

TEST(TableTest, hot_batches_test) {
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

  std::shared_ptr<Table> table_ptr = Table::Create("table_name", rel);
  Table& table = *table_ptr;

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

  auto slice = table.FirstBatch();
  auto rb1 =
      table.GetRowBatchSlice(slice, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  slice = table.NextBatch(slice);
  auto rb2 =
      table.GetRowBatchSlice(slice, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, hot_batches_w_compaction_test) {
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});

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
  int64_t rb1_size = 3 * sizeof(bool) + 3 * sizeof(int64_t);

  auto rb_wrapper_2 = std::make_unique<types::ColumnWrapperRecordBatch>();
  rb_wrapper_2->push_back(col1_in2_wrapper);
  rb_wrapper_2->push_back(col2_in2_wrapper);

  Table table("test_table", rel, 128 * 1024, rb1_size + 1);

  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_1)));
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_2)));

  auto slice = table.FirstBatch();
  auto rb1 =
      table.GetRowBatchSlice(slice, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));

  slice = table.NextBatch(slice);
  auto rb2 =
      table.GetRowBatchSlice(slice, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, find_batch_slice_greater_or_eq) {
  schema::Relation rel(std::vector<types::DataType>({types::DataType::TIME64NS}),
                       std::vector<std::string>({"time_"}));
  std::shared_ptr<Table> table_ptr = Table::Create("test_table", rel);
  Table& table = *table_ptr;

  std::vector<types::Time64NSValue> time_batch_1 = {2, 3, 4, 6};
  std::vector<types::Time64NSValue> time_batch_2 = {8, 8, 8};
  std::vector<types::Time64NSValue> time_batch_3 = {8, 9, 11};
  std::vector<types::Time64NSValue> time_batch_4 = {15, 16, 19};
  std::vector<types::Time64NSValue> time_batch_5 = {21, 21, 21};
  std::vector<types::Time64NSValue> time_batch_6 = {21, 23};

  auto wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_1);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_2);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_3);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_4);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_5);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_6);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  auto batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(0, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(0, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(3, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(5, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(3, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(3, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(6, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(3, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(3, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(8, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(4, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(6, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(10, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(9, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(9, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(13, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(10, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(12, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(21, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(13, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(15, batch_slice.uniq_row_end_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(24, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(-1, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(-1, batch_slice.uniq_row_end_idx);
}

TEST(TableTest, find_batch_slice_greater_or_eq_w_compaction) {
  schema::Relation rel(std::vector<types::DataType>({types::DataType::TIME64NS}),
                       std::vector<std::string>({"time_"}));
  int64_t compaction_size = 4 * sizeof(int64_t);
  Table table("test_table", rel, 128 * 1024, compaction_size);

  std::vector<types::Time64NSValue> time_batch_1 = {2, 3, 4, 6};
  std::vector<types::Time64NSValue> time_batch_2 = {8, 8, 8};
  std::vector<types::Time64NSValue> time_batch_3 = {8, 9, 11};
  std::vector<types::Time64NSValue> time_batch_4 = {15, 16, 19};
  std::vector<types::Time64NSValue> time_batch_5 = {21, 21, 21};
  std::vector<types::Time64NSValue> time_batch_6 = {21, 23};

  auto wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_1);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));
  auto batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(0, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(0, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(3, batch_slice.uniq_row_end_idx);
  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(5, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(3, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(3, batch_slice.uniq_row_end_idx);

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_2);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_3);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));
  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(6, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(3, batch_slice.uniq_row_start_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(8, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(4, batch_slice.uniq_row_start_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(10, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(9, batch_slice.uniq_row_start_idx);

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_4);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_5);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));

  wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
  col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(3);
  col_wrapper->Clear();
  col_wrapper->AppendFromVector(time_batch_6);
  wrapper_batch->push_back(col_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch)));
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(13, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(10, batch_slice.uniq_row_start_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(21, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(13, batch_slice.uniq_row_start_idx);

  batch_slice =
      table.FindBatchSliceGreaterThanOrEqual(24, arrow::default_memory_pool()).ConsumeValueOrDie();
  EXPECT_EQ(-1, batch_slice.uniq_row_start_idx);
  EXPECT_EQ(-1, batch_slice.uniq_row_end_idx);
}

TEST(TableTest, ToProto) {
  auto table = TestTable();
  table_store::schemapb::Table table_proto;
  EXPECT_OK(table->ToProto(&table_proto));

  std::string expected = R"(
relation {
  columns {
    column_name: "col1"
    column_type: FLOAT64
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "col2"
    column_type: INT64
    column_semantic_type: ST_NONE
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
  eow: false
  eos: false
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
  eow: true
  eos: true
  num_rows: 2
})";

  google::protobuf::util::MessageDifferencer differ;
  table_store::schemapb::Table expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected, &expected_proto));
  EXPECT_TRUE(differ.Compare(expected_proto, table_proto));
}

TEST(TableTest, transfer_empty_record_batch_test) {
  schema::Relation rel({types::DataType::INT64}, {"col1"});
  schema::RowDescriptor rd({types::DataType::INT64});

  std::shared_ptr<Table> table_ptr = Table::Create("test_table", rel);
  Table& table = *table_ptr;

  // ColumnWrapper with no columns should not be added to row batches.
  auto wrapper_batch_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_1)));

  EXPECT_EQ(table.GetTableStats().batches_added, 0);

  // Column wrapper with empty columns should not be added to row batches.
  auto wrapper_batch_2 = std::make_unique<types::ColumnWrapperRecordBatch>();
  auto col_wrapper_2 = std::make_shared<types::Time64NSValueColumnWrapper>(0);
  wrapper_batch_2->push_back(col_wrapper_2);
  EXPECT_OK(table.TransferRecordBatch(std::move(wrapper_batch_2)));

  EXPECT_EQ(table.GetTableStats().batches_added, 0);
}

TEST(TableTest, write_zero_row_row_batch) {
  schema::Relation rel({types::DataType::BOOLEAN, types::DataType::INT64}, {"col1", "col2"});
  schema::RowDescriptor rd({types::DataType::BOOLEAN, types::DataType::INT64});

  std::shared_ptr<Table> table_ptr = Table::Create("test_table", rel);

  auto result = schema::RowBatch::WithZeroRows(rd, /*eow*/ false, /*eos*/ false);
  ASSERT_OK(result);
  auto rb_ptr = result.ConsumeValueOrDie();

  EXPECT_OK(table_ptr->WriteRowBatch(*rb_ptr));
  // Row batch with 0 rows won't be written.
  EXPECT_EQ(table_ptr->GetTableStats().batches_added, 0);
}

TEST(TableTest, threaded) {
  schema::Relation rel({types::DataType::TIME64NS}, {"time_"});
  schema::RowDescriptor rd({types::DataType::TIME64NS});
  std::shared_ptr<Table> table_ptr =
      std::make_shared<Table>("test_table", rel, 8 * 1024 * 1024, 5 * 1024);

  int64_t max_time_counter = 1024 * 1024;

  auto done = std::make_shared<absl::Notification>();

  std::thread compaction_thread([table_ptr, done]() {
    while (!done->WaitForNotificationWithTimeout(absl::Milliseconds(50))) {
      EXPECT_OK(table_ptr->CompactHotToCold(arrow::default_memory_pool()));
    }
    // Do one last compaction after writer thread has finished writing.
    EXPECT_OK(table_ptr->CompactHotToCold(arrow::default_memory_pool()));
  });

  std::thread writer_thread([table_ptr, done, max_time_counter]() {
    std::default_random_engine gen;
    std::uniform_int_distribution<int64_t> dist(256, 1024);
    int64_t time_counter = 0;
    while (time_counter < max_time_counter) {
      int64_t batch_size = dist(gen);
      if (time_counter + batch_size > max_time_counter) {
        batch_size = max_time_counter - time_counter;
      }
      std::vector<types::Time64NSValue> time_col(batch_size);
      for (int row_idx = 0; row_idx < batch_size; row_idx++) {
        time_col[row_idx] = time_counter++;
      }
      auto wrapper_batch = std::make_unique<types::ColumnWrapperRecordBatch>();
      auto col_wrapper = std::make_shared<types::Time64NSValueColumnWrapper>(batch_size);
      col_wrapper->Clear();
      col_wrapper->AppendFromVector(time_col);
      wrapper_batch->push_back(col_wrapper);
      EXPECT_OK(table_ptr->TransferRecordBatch(std::move(wrapper_batch)));
    }
    done->Notify();
  });

  std::thread reader_thread([table_ptr, done, max_time_counter]() {
    int64_t time_counter = 0;
    auto slice = table_ptr->FirstBatch();
    while (!slice.IsValid()) {
      slice = table_ptr->FirstBatch();
    }

    while (time_counter < max_time_counter &&
           !done->WaitForNotificationWithTimeout(absl::Milliseconds(1))) {
      EXPECT_TRUE(slice.IsValid());
      auto batch =
          table_ptr->GetRowBatchSlice(slice, {0}, arrow::default_memory_pool()).ConsumeValueOrDie();
      auto time_col = std::static_pointer_cast<arrow::Int64Array>(batch->ColumnAt(0));
      for (int i = 0; i < time_col->length(); ++i) {
        EXPECT_EQ(time_counter, time_col->Value(i));
        time_counter++;
      }
      slice = table_ptr->NextBatch(slice);
    }

    while (time_counter < max_time_counter && slice.IsValid()) {
      auto batch =
          table_ptr->GetRowBatchSlice(slice, {0}, arrow::default_memory_pool()).ConsumeValueOrDie();
      auto time_col = std::static_pointer_cast<arrow::Int64Array>(batch->ColumnAt(0));
      for (int i = 0; i < time_col->length(); ++i) {
        EXPECT_EQ(time_counter, time_col->Value(i));
        time_counter++;
      }
      slice = table_ptr->NextBatch(slice);
    }

    EXPECT_EQ(time_counter, max_time_counter);
  });

  writer_thread.join();
  compaction_thread.join();
  reader_thread.join();
}

TEST(TableTest, NextBatch_generation_bug) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char);
  Table table("test_table", rel, rb1_size, rb1_size);

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));

  auto slice = table.FirstBatch();
  // Force cold expiration.
  EXPECT_OK(table.WriteRowBatch(rb1));
  // Call NextBatch on slice, which before the bug fix updated it's generation when it shouldn't
  // have.
  table.NextBatch(slice);
  // GetRowBatchSlice should return invalidargument since the slice was expired. Prior to the bug
  // fix this would segfault.
  EXPECT_NOT_OK(table.GetRowBatchSlice(slice, {0, 1}, arrow::default_memory_pool()));
}

}  // namespace table_store
}  // namespace px
