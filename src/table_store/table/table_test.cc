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
#include "src/shared/types/typespb/types.pb.h"
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
  PX_CHECK_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  PX_CHECK_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  PX_CHECK_OK(table->WriteRowBatch(rb1));

  auto rb2 = schema::RowBatch(schema::RowDescriptor(rel.col_types()), 2);
  std::vector<types::Float64Value> col1_in2 = {0.1, 5.1};
  std::vector<types::Int64Value> col2_in2 = {5, 6};
  PX_CHECK_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  PX_CHECK_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  PX_CHECK_OK(table->WriteRowBatch(rb2));

  return table;
}

}  // namespace

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

  Table::Cursor cursor(table_ptr.get());

  auto actual_rb1 = cursor.GetNextRowBatch(std::vector<int64_t>({0, 1})).ConsumeValueOrDie();
  EXPECT_TRUE(
      actual_rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(
      actual_rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto actual_rb2 = cursor.GetNextRowBatch(std::vector<int64_t>({0, 1})).ConsumeValueOrDie();
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
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char) + 3 * sizeof(uint32_t);

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char) + 2 * sizeof(uint32_t);

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
  int64_t rb3_size = 3 * sizeof(int64_t) + 9 * sizeof(char) + 3 * sizeof(uint32_t);

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
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char) + 3 * sizeof(uint32_t);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char) + 2 * sizeof(uint32_t);

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
  int64_t rb3_size = 3 * sizeof(int64_t) + 9 * sizeof(char) + 3 * sizeof(uint32_t);

  // Make minimum batch size rb1_size + rb2_size so that compaction causes 2 of the 3 batches to
  // be compacted into cold.
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

  Table table("test_table", rel, 80);

  schema::RowBatch rb1(rd, 3);
  std::vector<types::Int64Value> col1_rb1 = {4, 5, 10};
  std::vector<types::StringValue> col2_rb1 = {"hello", "abc", "defg"};
  auto col1_rb1_arrow = types::ToArrow(col1_rb1, arrow::default_memory_pool());
  auto col2_rb1_arrow = types::ToArrow(col2_rb1, arrow::default_memory_pool());
  EXPECT_OK(rb1.AddColumn(col1_rb1_arrow));
  EXPECT_OK(rb1.AddColumn(col2_rb1_arrow));
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char) + 3 * sizeof(uint32_t);

  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char) + 2 * sizeof(uint32_t);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size);

  schema::RowBatch rb3(rd, 2);
  std::vector<types::Int64Value> col1_rb3 = {4, 5};
  std::vector<types::StringValue> col2_rb3 = {"longerstring", "hellohellohello"};
  auto col1_rb3_arrow = types::ToArrow(col1_rb3, arrow::default_memory_pool());
  auto col2_rb3_arrow = types::ToArrow(col2_rb3, arrow::default_memory_pool());
  EXPECT_OK(rb3.AddColumn(col1_rb3_arrow));
  EXPECT_OK(rb3.AddColumn(col2_rb3_arrow));
  int64_t rb3_size = 2 * sizeof(int64_t) + 27 * sizeof(char) + 2 * sizeof(uint32_t);

  EXPECT_OK(table.WriteRowBatch(rb3));
  EXPECT_EQ(table.GetTableStats().bytes, rb2_size + rb3_size);

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
  int64_t rb4_size = 1 * sizeof(int64_t) + 1 * sizeof(char) + 1 * sizeof(uint32_t);

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
  int64_t rb5_size = 5 * sizeof(int64_t) + 20 * sizeof(char) + 5 * sizeof(uint32_t);

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
  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char) + 3 * sizeof(uint32_t);

  schema::RowBatch rb2(rd, 2);
  std::vector<types::Int64Value> col1_rb2 = {4, 5};
  std::vector<types::StringValue> col2_rb2 = {"a", "bc"};
  auto col1_rb2_arrow = types::ToArrow(col1_rb2, arrow::default_memory_pool());
  auto col2_rb2_arrow = types::ToArrow(col2_rb2, arrow::default_memory_pool());
  EXPECT_OK(rb2.AddColumn(col1_rb2_arrow));
  EXPECT_OK(rb2.AddColumn(col2_rb2_arrow));
  int64_t rb2_size = 2 * sizeof(int64_t) + 3 * sizeof(char) + 2 * sizeof(uint32_t);

  schema::RowBatch rb3(rd, 2);
  std::vector<types::Int64Value> col1_rb3 = {4, 5};
  std::vector<types::StringValue> col2_rb3 = {"longerstring", "hellohellohello"};
  auto col1_rb3_arrow = types::ToArrow(col1_rb3, arrow::default_memory_pool());
  auto col2_rb3_arrow = types::ToArrow(col2_rb3, arrow::default_memory_pool());
  EXPECT_OK(rb3.AddColumn(col1_rb3_arrow));
  EXPECT_OK(rb3.AddColumn(col2_rb3_arrow));
  int64_t rb3_size = 2 * sizeof(int64_t) + 27 * sizeof(char) + 2 * sizeof(uint32_t);

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
  int64_t rb4_size = 1 * sizeof(int64_t) + 1 * sizeof(char) + 1 * sizeof(uint32_t);

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
  int64_t rb5_size = 5 * sizeof(int64_t) + 20 * sizeof(char) + 5 * sizeof(uint32_t);

  Table table("test_table", rel, 80, 40);
  EXPECT_OK(table.WriteRowBatch(rb1));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size);

  EXPECT_OK(table.WriteRowBatch(rb2));
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));
  EXPECT_EQ(table.GetTableStats().bytes, rb1_size + rb2_size);

  EXPECT_OK(table.WriteRowBatch(rb3));
  EXPECT_EQ(table.GetTableStats().bytes, rb2_size + rb3_size);

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

  Table::Cursor cursor(table_ptr.get());
  auto rb_or_s = cursor.GetNextRowBatch({0, 1});
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

  Table::Cursor cursor(table_ptr.get());
  auto rb1 = cursor.GetNextRowBatch({0, 1}).ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  auto rb2 = cursor.GetNextRowBatch({0, 1}).ConsumeValueOrDie();
  ASSERT_NE(rb2, nullptr);
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

  Table table("test_table", rel, 128 * 1024, rb1_size);

  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_1)));
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_2)));

  Table::Cursor cursor(&table);
  auto rb1 = cursor.GetNextRowBatch({0, 1}).ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in1, arrow::default_memory_pool())));

  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));

  auto rb2 = cursor.GetNextRowBatch({0, 1}).ConsumeValueOrDie();
  EXPECT_TRUE(rb2->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb2->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

TEST(TableTest, find_rowid_from_time_first_greater_than_or_equal) {
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

  EXPECT_EQ(0, table.FindRowIDFromTimeFirstGreaterThanOrEqual(0));

  EXPECT_EQ(3, table.FindRowIDFromTimeFirstGreaterThanOrEqual(5));

  EXPECT_EQ(3, table.FindRowIDFromTimeFirstGreaterThanOrEqual(6));

  EXPECT_EQ(4, table.FindRowIDFromTimeFirstGreaterThanOrEqual(8));

  EXPECT_EQ(9, table.FindRowIDFromTimeFirstGreaterThanOrEqual(10));

  EXPECT_EQ(10, table.FindRowIDFromTimeFirstGreaterThanOrEqual(13));

  EXPECT_EQ(13, table.FindRowIDFromTimeFirstGreaterThanOrEqual(21));

  // If the time is not in the table it returns the RowID after the end of the table (which is the
  // number of rows that have been added to the table).
  EXPECT_EQ(time_batch_1.size() + time_batch_2.size() + time_batch_3.size() + time_batch_4.size() +
                time_batch_5.size() + time_batch_6.size(),
            table.FindRowIDFromTimeFirstGreaterThanOrEqual(24));
}

TEST(TableTest, find_rowid_from_time_first_greater_than_or_equal_with_compaction) {
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

  // Run Compaction.
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));
  EXPECT_EQ(0, table.FindRowIDFromTimeFirstGreaterThanOrEqual(0));
  EXPECT_EQ(3, table.FindRowIDFromTimeFirstGreaterThanOrEqual(5));

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

  // Run Compaction.
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));
  EXPECT_EQ(3, table.FindRowIDFromTimeFirstGreaterThanOrEqual(6));

  EXPECT_EQ(4, table.FindRowIDFromTimeFirstGreaterThanOrEqual(8));

  EXPECT_EQ(9, table.FindRowIDFromTimeFirstGreaterThanOrEqual(10));

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
  // Run Compaction.
  EXPECT_OK(table.CompactHotToCold(arrow::default_memory_pool()));

  EXPECT_EQ(10, table.FindRowIDFromTimeFirstGreaterThanOrEqual(13));

  EXPECT_EQ(13, table.FindRowIDFromTimeFirstGreaterThanOrEqual(21));

  // If the time is not in the table it returns the RowID after the end of the table (which is the
  // number of rows that have been added to the table).
  EXPECT_EQ(time_batch_1.size() + time_batch_2.size() + time_batch_3.size() + time_batch_4.size() +
                time_batch_5.size() + time_batch_6.size(),
            table.FindRowIDFromTimeFirstGreaterThanOrEqual(24));
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

class NotifyOnDeath {
 public:
  explicit NotifyOnDeath(absl::Notification* notification) : notification_(notification) {}
  ~NotifyOnDeath() {
    if (!notification_->HasBeenNotified()) {
      notification_->Notify();
    }
  }

 private:
  absl::Notification* notification_;
};

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

  // Create the cursor before the write thread starts, to ensure that we get every row of the table.
  Table::Cursor cursor(table_ptr.get(), Table::Cursor::StartSpec{},
                       Table::Cursor::StopSpec{Table::Cursor::StopSpec::StopType::Infinite});

  std::thread writer_thread([table_ptr, done, max_time_counter]() {
    std::default_random_engine gen;
    std::uniform_int_distribution<int64_t> dist(256, 1024);
    int64_t time_counter = 0;
    // This RAII wrapper around done, will notify threads waiting on done when it goes out of
    // scope if done->Notify hasn't been called yet. This way if the writer thread dies for some
    // reason the test will fail immediately instead of timing out.
    NotifyOnDeath notifier(done.get());
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

  std::thread reader_thread([table_ptr, done, max_time_counter, &cursor]() {
    int64_t time_counter = 0;

    // Wait for the writer thread to push some data to the table.
    while (!cursor.NextBatchReady()) {
      std::this_thread::sleep_for(std::chrono::microseconds{50});
    }

    // Loop over slices whilst the writer is still writing and we haven't seen all of the data
    // yet.
    while (time_counter < max_time_counter &&
           !done->WaitForNotificationWithTimeout(absl::Microseconds(1))) {
      EXPECT_TRUE(cursor.NextBatchReady());
      auto batch = cursor.GetNextRowBatch({0}).ConsumeValueOrDie();
      auto time_col = std::static_pointer_cast<arrow::Int64Array>(batch->ColumnAt(0));
      for (int i = 0; i < time_col->length(); ++i) {
        EXPECT_EQ(time_counter, time_col->Value(i));
        time_counter++;
      }
      // If the reader gets ahead of the writer we have to wait for the writer to write more
      // data. We check the done notifcation here so that if the writer fails to write all the data
      // for some reason, we still exit.
      while (time_counter < max_time_counter && !cursor.NextBatchReady() &&
             !done->WaitForNotificationWithTimeout(absl::Milliseconds(1))) {
      }
    }

    // Now that the writer is finished move the stop of the cursor to the current end of the table.
    cursor.UpdateStopSpec(Table::Cursor::StopSpec{Table::Cursor::StopSpec::CurrentEndOfTable});

    // Once the writer is finished, we loop over the remaining data in the table.
    while (time_counter < max_time_counter && !cursor.Done()) {
      auto batch = cursor.GetNextRowBatch({0}).ConsumeValueOrDie();
      auto time_col = std::static_pointer_cast<arrow::Int64Array>(batch->ColumnAt(0));
      for (int i = 0; i < time_col->length(); ++i) {
        ASSERT_EQ(time_counter, time_col->Value(i));
        time_counter++;
      }
    }

    EXPECT_EQ(time_counter, max_time_counter);
  });

  writer_thread.join();
  compaction_thread.join();
  reader_thread.join();
}

// This test was add when `NextBatch` and `BatchSlice`'s were still around, and there was a bug with
// generation handling of `BatchSlice`'s. Maintaining so as not to decrease test coverage, but this
// bug should no longer even be plausible.
TEST(TableTest, NextBatch_generation_bug) {
  auto rd = schema::RowDescriptor({types::DataType::INT64, types::DataType::STRING});
  schema::Relation rel(rd.types(), {"col1", "col2"});

  int64_t rb1_size = 3 * sizeof(int64_t) + 12 * sizeof(char) + 3 * sizeof(uint32_t);
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

  Table::Cursor cursor(&table, Table::Cursor::StartSpec{}, Table::Cursor::StopSpec{});
  // Force cold expiration.
  EXPECT_OK(table.WriteRowBatch(rb1));
  // GetNextRowBatch should return invalidargument since the batch was expired.
  EXPECT_NOT_OK(cursor.GetNextRowBatch({0, 1}));
}

TEST(TableTest, GetNextRowBatch_after_expiry) {
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
  int64_t rb2_size = 2 * sizeof(bool) + 2 * sizeof(int64_t);

  Table table("test_table", rel, rb1_size + rb2_size, rb1_size);

  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_1)));
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_2)));

  Table::Cursor cursor(&table);

  // This write will expire the first batch.
  auto rb_wrapper_1_copy = std::make_unique<types::ColumnWrapperRecordBatch>();
  rb_wrapper_1_copy->push_back(col1_in1_wrapper);
  rb_wrapper_1_copy->push_back(col2_in1_wrapper);
  EXPECT_OK(table.TransferRecordBatch(std::move(rb_wrapper_1_copy)));

  // GetNextRowBatch should return the second row batch since the first one was expired by the third
  // write.
  auto rb1 = cursor.GetNextRowBatch({0, 1}).ConsumeValueOrDie();
  EXPECT_TRUE(rb1->ColumnAt(0)->Equals(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_TRUE(rb1->ColumnAt(1)->Equals(types::ToArrow(col2_in2, arrow::default_memory_pool())));
}

struct CursorTestCase {
  std::string name;
  std::vector<std::vector<int64_t>> initial_time_batches;
  Table::Cursor::StartSpec start_spec;
  Table::Cursor::StopSpec stop_spec;
  struct Action {
    enum ActionType {
      ExpectBatch,
      AddBatchToTable,
    } type;
    std::vector<int64_t> times;
  };
  std::vector<Action> actions;
  bool expect_cursor_exhausted_after_actions;
};
using CursorTestActionType = CursorTestCase::Action::ActionType;

class CursorTableTest : public ::testing::Test,
                        public ::testing::WithParamInterface<CursorTestCase> {
 protected:
  void SetUp() override {
    ::testing::Test::SetUp();
    test_case_ = GetParam();

    rel_ = std::make_unique<schema::Relation>(std::vector<types::DataType>{types::TIME64NS},
                                              std::vector<std::string>{"time_"});
    table_ptr_ = Table::Create("test_table", *rel_);

    for (const auto& batch : test_case_.initial_time_batches) {
      WriteBatch(batch);
    }

    cursor_ = std::make_unique<Table::Cursor>(table_ptr_.get(), test_case_.start_spec,
                                              test_case_.stop_spec);
  }

  void WriteBatch(const std::vector<int64_t>& times) {
    auto rb = schema::RowBatch(schema::RowDescriptor(rel_->col_types()),
                               static_cast<int64_t>(times.size()));
    std::vector<types::Time64NSValue> col(times.begin(), times.end());
    EXPECT_OK(rb.AddColumn(types::ToArrow(col, arrow::default_memory_pool())));
    EXPECT_OK(table_ptr_->WriteRowBatch(rb));
  }

  void ExpectBatch(const std::vector<int64_t>& times) {
    auto actual_batch_or_s = cursor_->GetNextRowBatch(std::vector<int64_t>{0});
    ASSERT_OK(actual_batch_or_s);
    auto actual_batch = actual_batch_or_s.ConsumeValueOrDie();

    auto actual_iterator =
        types::ArrowArrayIterator<types::TIME64NS>(actual_batch->ColumnAt(0).get());
    auto actual_vec = std::vector<int64_t>{actual_iterator.begin(), actual_iterator.end()};

    EXPECT_TRUE(actual_batch->ColumnAt(0)->Equals(
        types::ToArrow(std::vector<types::Time64NSValue>{times.begin(), times.end()},
                       arrow::default_memory_pool())))
        << "expected " << ::testing::PrintToString(times) << " received "
        << ::testing::PrintToString(actual_vec);
  }

  CursorTestCase test_case_;
  std::unique_ptr<schema::Relation> rel_;
  std::shared_ptr<Table> table_ptr_;
  std::unique_ptr<Table::Cursor> cursor_;
};

TEST_P(CursorTableTest, cursor_test) {
  for (const auto& action : test_case_.actions) {
    if (action.type == CursorTestActionType::ExpectBatch) {
      EXPECT_FALSE(cursor_->Done());
      EXPECT_TRUE(cursor_->NextBatchReady());
      ExpectBatch(action.times);
    }
    if (action.type == CursorTestActionType::AddBatchToTable) {
      WriteBatch(action.times);
    }
  }
  if (test_case_.expect_cursor_exhausted_after_actions) {
    EXPECT_TRUE(cursor_->Done());
  }
}

using StartType = Table::Cursor::StartSpec::StartType;
using StopType = Table::Cursor::StopSpec::StopType;

INSTANTIATE_TEST_SUITE_P(CursorTableTestSuite, CursorTableTest,
                         ::testing::ValuesIn(std::vector<CursorTestCase>{
                             {
                                 "CurrentStartOfTable_CurrentEndOfTable",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::CurrentStartOfTable,
                                 },
                                 {
                                     StopType::CurrentEndOfTable,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     // This batch should never be returned by the cursor, since it
                                     // was added after cursor creation.
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                 },
                                 // Check that the cursor is exhausted to make sure that the {7, 8}
                                 // batch is not included.
                                 true,
                             },
                             {
                                 "CurrentStartOfTable_Infinite",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::CurrentStartOfTable,
                                 },
                                 {
                                     StopType::Infinite,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {20, 22},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {20, 22},
                                     },
                                 },
                                 // Infinite cursor shouldn't ever be exhausted.
                                 false,
                             },
                             {
                                 "CurrentStartOfTable_StopAtTimeOrEndOfTable",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::CurrentStartOfTable,
                                 },
                                 {
                                     StopType::StopAtTimeOrEndOfTable,
                                     5,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 11},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5},
                                     },
                                 },
                                 // Cursor should be exhausted.
                                 true,
                             },
                             {
                                 "CurrentStartOfTable_StopAtTimeOrEndOfTable_future_time",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::CurrentStartOfTable,
                                 },
                                 {
                                     StopType::StopAtTimeOrEndOfTable,
                                     7,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                 },
                                 true,
                             },
                             {
                                 "CurrentStartOfTable_StopAtTime",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::CurrentStartOfTable,
                                 },
                                 {
                                     StopType::StopAtTime,
                                     5,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 11},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5},
                                     },
                                 },
                                 // Cursor should be exhausted.
                                 true,
                             },
                             {
                                 "CurrentStartOfTable_StopAtTime_future_time",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::CurrentStartOfTable,
                                 },
                                 {
                                     StopType::StopAtTime,
                                     10,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 10, 10},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {10, 10, 10},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 10, 11},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {10, 10},
                                     },
                                 },
                                 // Cursor should be exhausted after seeing the 11 record.
                                 true,
                             },
                             {
                                 "StartAtTime_CurrentEndOfTable",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     3,
                                 },
                                 {
                                     StopType::CurrentEndOfTable,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {3},
                                     },
                                     // This batch should never be returned by the cursor, since it
                                     // was added after cursor creation.
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                 },
                                 // Check that the cursor is exhausted to make sure that the {7, 8}
                                 // batch is not included.
                                 true,
                             },
                             {
                                 "StartAtTime_CurrentEndOfTable_future_start_time",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     7,
                                 },
                                 {
                                     StopType::CurrentEndOfTable,
                                 },
                                 {
                                     // This batch should never be returned by the cursor, since it
                                     // was added after cursor creation.
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                 },
                                 // Check that the cursor is exhausted to make sure that the {7, 8}
                                 // batch is not included.
                                 true,
                             },
                             {
                                 "StartAtTime_Infinite",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     5,
                                 },
                                 {
                                     StopType::Infinite,
                                 },
                                 {
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {20, 22},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {20, 22},
                                     },
                                 },
                                 // Infinite cursor shouldn't ever be exhausted.
                                 false,
                             },
                             {
                                 "StartAtTime_StopAtTimeOrEndOfTable",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     2,
                                 },
                                 {
                                     StopType::StopAtTimeOrEndOfTable,
                                     5,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 11},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5},
                                     },
                                 },
                                 // Cursor should be exhausted.
                                 true,
                             },
                             {
                                 "StartAtTime_StopAtTimeOrEndOfTable_future_time",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     1,
                                 },
                                 {
                                     StopType::StopAtTimeOrEndOfTable,
                                     7,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {1, 2, 3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5, 6},
                                     },
                                 },
                                 true,
                             },
                             {
                                 "StartAtTime_StopAtTime",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     3,
                                 },
                                 {
                                     StopType::StopAtTime,
                                     5,
                                 },
                                 {
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {3},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 11},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {5},
                                     },
                                 },
                                 // Cursor should be exhausted.
                                 true,
                             },
                             {
                                 "StartAtTime_StopAtTime_future_time",
                                 {
                                     {1, 2, 3},
                                     {5, 6},
                                 },
                                 {
                                     StartType::StartAtTime,
                                     7,
                                 },
                                 {
                                     StopType::StopAtTime,
                                     10,
                                 },
                                 {
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {7, 8},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 10, 10},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {10, 10, 10},
                                     },
                                     {
                                         CursorTestActionType::AddBatchToTable,
                                         {10, 10, 11},
                                     },
                                     {
                                         CursorTestActionType::ExpectBatch,
                                         {10, 10},
                                     },
                                 },
                                 // Cursor should be exhausted after seeing the 11 record.
                                 true,
                             },
                         }),
                         [](const ::testing::TestParamInfo<CursorTableTest::ParamType>& info) {
                           return info.param.name;
                         });

}  // namespace table_store
}  // namespace px
