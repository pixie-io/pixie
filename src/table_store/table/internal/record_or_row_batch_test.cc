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

#include <memory>
#include <numeric>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table/internal/record_or_row_batch.h"
#include "src/table_store/table/internal/test_utils.h"

namespace px {
namespace table_store {
namespace internal {

class RecordOrRowBatchTest : public RecordOrRowBatchParamTest {
 protected:
  void SetUp() override {
    RecordOrRowBatchParamTest::SetUp();
    times_ = {9, 10, 20, 25};
    bools_ = {true, false, true, false};
    strings_ = {
        "one", "very", "long",
        "string. This one. This is the very long one. I'm going to keep going, to make it even "
        "longer, and longer"};
    ColSizes col_sizes;
    std::tie(rb_, col_sizes) = MakeRecordOrRowBatch(times_, bools_, strings_);

    time_col_idx_ = 0;
    strings_total_length_ = col_sizes[2];
  }
  std::vector<types::Time64NSValue> times_;
  std::vector<types::BoolValue> bools_;
  std::vector<types::StringValue> strings_;
  std::unique_ptr<RecordOrRowBatch> rb_;
  int64_t time_col_idx_;
  size_t strings_total_length_;
};

TEST_P(RecordOrRowBatchTest, Length) { EXPECT_EQ(4, rb_->Length()); }
TEST_P(RecordOrRowBatchTest, RemovePrefix_Length) {
  rb_->RemovePrefix(2);
  EXPECT_EQ(2, rb_->Length());
}

TEST_P(RecordOrRowBatchTest, FindTimeFirstGreaterThanOrEqual) {
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 0));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 9));
  EXPECT_EQ(1, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 10));
  EXPECT_EQ(2, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 19));
  EXPECT_EQ(-1, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 26));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_FindTimeFirstGreaterThanOrEqual) {
  rb_->RemovePrefix(2);

  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 0));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 9));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 10));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 19));
  EXPECT_EQ(1, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 25));
  EXPECT_EQ(-1, rb_->FindTimeFirstGreaterThanOrEqual(time_col_idx_, 26));
}

TEST_P(RecordOrRowBatchTest, FindTimeFirstGreaterThan) {
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThan(time_col_idx_, 0));
  EXPECT_EQ(1, rb_->FindTimeFirstGreaterThan(time_col_idx_, 9));
  EXPECT_EQ(2, rb_->FindTimeFirstGreaterThan(time_col_idx_, 10));
  EXPECT_EQ(2, rb_->FindTimeFirstGreaterThan(time_col_idx_, 19));
  EXPECT_EQ(-1, rb_->FindTimeFirstGreaterThan(time_col_idx_, 25));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_FindTimeFirstGreaterThan) {
  rb_->RemovePrefix(2);

  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThan(time_col_idx_, 0));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThan(time_col_idx_, 9));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThan(time_col_idx_, 10));
  EXPECT_EQ(0, rb_->FindTimeFirstGreaterThan(time_col_idx_, 19));
  EXPECT_EQ(1, rb_->FindTimeFirstGreaterThan(time_col_idx_, 24));
  EXPECT_EQ(-1, rb_->FindTimeFirstGreaterThan(time_col_idx_, 25));
}

TEST_P(RecordOrRowBatchTest, GetTimeValue) {
  EXPECT_EQ(9, rb_->GetTimeValue(time_col_idx_, 0));
  EXPECT_EQ(10, rb_->GetTimeValue(time_col_idx_, 1));
  EXPECT_EQ(20, rb_->GetTimeValue(time_col_idx_, 2));
  EXPECT_EQ(25, rb_->GetTimeValue(time_col_idx_, 3));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_GetTimeValue) {
  rb_->RemovePrefix(2);
  EXPECT_EQ(20, rb_->GetTimeValue(time_col_idx_, 0));
  EXPECT_EQ(25, rb_->GetTimeValue(time_col_idx_, 1));
}

TEST_P(RecordOrRowBatchTest, AddBatchSliceToRowBatch) {
  schema::RowBatch rb0(schema::RowDescriptor(rel_->col_types()), 2);
  EXPECT_OK(rb_->AddBatchSliceToRowBatch(0, 2, {0, 1, 2}, &rb0));

  EXPECT_TRUE(
      rb0.ColumnAt(0)->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(0, 2)));
  EXPECT_TRUE(
      rb0.ColumnAt(1)->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(0, 2)));
  EXPECT_TRUE(
      rb0.ColumnAt(2)->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(0, 2)));

  schema::RowBatch rb1(schema::RowDescriptor(rel_->col_types()), 2);
  EXPECT_OK(rb_->AddBatchSliceToRowBatch(1, 2, {0, 1, 2}, &rb1));

  EXPECT_TRUE(
      rb1.ColumnAt(0)->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(1, 2)));
  EXPECT_TRUE(
      rb1.ColumnAt(1)->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(1, 2)));
  EXPECT_TRUE(
      rb1.ColumnAt(2)->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(1, 2)));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_AddBatchSliceToRowBatch) {
  rb_->RemovePrefix(1);

  schema::RowBatch rb0(schema::RowDescriptor(rel_->col_types()), 2);
  EXPECT_OK(rb_->AddBatchSliceToRowBatch(0, 2, {0, 1, 2}, &rb0));

  EXPECT_TRUE(
      rb0.ColumnAt(0)->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(1, 2)));
  EXPECT_TRUE(
      rb0.ColumnAt(1)->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(1, 2)));
  EXPECT_TRUE(
      rb0.ColumnAt(2)->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(1, 2)));

  schema::RowBatch rb1(schema::RowDescriptor(rel_->col_types()), 1);
  EXPECT_OK(rb_->AddBatchSliceToRowBatch(1, 1, {0, 1, 2}, &rb1));

  EXPECT_TRUE(
      rb1.ColumnAt(0)->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(2, 1)));
  EXPECT_TRUE(
      rb1.ColumnAt(1)->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(2, 1)));
  EXPECT_TRUE(
      rb1.ColumnAt(2)->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(2, 1)));
}

TEST_P(RecordOrRowBatchTest, UnsafeAppendColumnToBuilder) {
  auto time_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::TIME64NS, arrow::default_memory_pool());
  EXPECT_OK(time_builder->Reserve(4));
  auto bool_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::BOOLEAN, arrow::default_memory_pool());
  EXPECT_OK(bool_builder->Reserve(4));
  auto string_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::STRING, arrow::default_memory_pool());
  EXPECT_OK(string_builder->Reserve(4));
  EXPECT_OK(string_builder->ReserveData(strings_total_length_));

  rb_->UnsafeAppendColumnToBuilder(time_builder.get(), types::DataType::TIME64NS, 0, 0, 4);
  rb_->UnsafeAppendColumnToBuilder(bool_builder.get(), types::DataType::BOOLEAN, 1, 0, 4);
  rb_->UnsafeAppendColumnToBuilder(string_builder.get(), types::DataType::STRING, 2, 0, 4);

  std::shared_ptr<arrow::Array> time_col;
  std::shared_ptr<arrow::Array> bool_col;
  std::shared_ptr<arrow::Array> string_col;
  EXPECT_OK(time_builder->Finish(&time_col));
  EXPECT_OK(bool_builder->Finish(&bool_col));
  EXPECT_OK(string_builder->Finish(&string_col));

  EXPECT_TRUE(time_col->Equals(types::ToArrow(times_, arrow::default_memory_pool())));
  EXPECT_TRUE(bool_col->Equals(types::ToArrow(bools_, arrow::default_memory_pool())));
  EXPECT_TRUE(string_col->Equals(types::ToArrow(strings_, arrow::default_memory_pool())));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_UnsafeAppendColumnToBuilder) {
  rb_->RemovePrefix(1);

  auto time_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::TIME64NS, arrow::default_memory_pool());
  EXPECT_OK(time_builder->Reserve(3));
  auto bool_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::BOOLEAN, arrow::default_memory_pool());
  EXPECT_OK(bool_builder->Reserve(3));
  auto string_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::STRING, arrow::default_memory_pool());
  EXPECT_OK(string_builder->Reserve(3));
  EXPECT_OK(string_builder->ReserveData(strings_total_length_));

  rb_->UnsafeAppendColumnToBuilder(time_builder.get(), types::DataType::TIME64NS, 0, 0, 3);
  rb_->UnsafeAppendColumnToBuilder(bool_builder.get(), types::DataType::BOOLEAN, 1, 0, 3);
  rb_->UnsafeAppendColumnToBuilder(string_builder.get(), types::DataType::STRING, 2, 0, 3);

  std::shared_ptr<arrow::Array> time_col;
  std::shared_ptr<arrow::Array> bool_col;
  std::shared_ptr<arrow::Array> string_col;
  EXPECT_OK(time_builder->Finish(&time_col));
  EXPECT_OK(bool_builder->Finish(&bool_col));
  EXPECT_OK(string_builder->Finish(&string_col));

  EXPECT_TRUE(time_col->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(1, 3)));
  EXPECT_TRUE(bool_col->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(1, 3)));
  EXPECT_TRUE(
      string_col->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(1, 3)));
}

TEST_P(RecordOrRowBatchTest, UnsafeAppendColumnToBuilderSliced) {
  auto time_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::TIME64NS, arrow::default_memory_pool());
  EXPECT_OK(time_builder->Reserve(2));
  auto bool_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::BOOLEAN, arrow::default_memory_pool());
  EXPECT_OK(bool_builder->Reserve(2));
  auto string_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::STRING, arrow::default_memory_pool());
  EXPECT_OK(string_builder->Reserve(2));
  size_t str_total_size = strings_[1].size() + strings_[2].size();
  EXPECT_OK(string_builder->ReserveData(str_total_size));

  rb_->UnsafeAppendColumnToBuilder(time_builder.get(), types::DataType::TIME64NS, 0, 1, 3);
  rb_->UnsafeAppendColumnToBuilder(bool_builder.get(), types::DataType::BOOLEAN, 1, 1, 3);
  rb_->UnsafeAppendColumnToBuilder(string_builder.get(), types::DataType::STRING, 2, 1, 3);

  std::shared_ptr<arrow::Array> time_col;
  std::shared_ptr<arrow::Array> bool_col;
  std::shared_ptr<arrow::Array> string_col;
  EXPECT_OK(time_builder->Finish(&time_col));
  EXPECT_OK(bool_builder->Finish(&bool_col));
  EXPECT_OK(string_builder->Finish(&string_col));

  EXPECT_TRUE(time_col->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(1, 2)));
  EXPECT_TRUE(bool_col->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(1, 2)));
  EXPECT_TRUE(
      string_col->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(1, 2)));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_UnsafeAppendColumnToBuilderSliced) {
  rb_->RemovePrefix(1);

  auto time_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::TIME64NS, arrow::default_memory_pool());
  EXPECT_OK(time_builder->Reserve(2));
  auto bool_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::BOOLEAN, arrow::default_memory_pool());
  EXPECT_OK(bool_builder->Reserve(2));
  auto string_builder =
      types::MakeTypeErasedArrowBuilder(types::DataType::STRING, arrow::default_memory_pool());
  EXPECT_OK(string_builder->Reserve(2));
  size_t str_total_size = strings_[2].size() + strings_[3].size();
  EXPECT_OK(string_builder->ReserveData(str_total_size));

  rb_->UnsafeAppendColumnToBuilder(time_builder.get(), types::DataType::TIME64NS, 0, 1, 3);
  rb_->UnsafeAppendColumnToBuilder(bool_builder.get(), types::DataType::BOOLEAN, 1, 1, 3);
  rb_->UnsafeAppendColumnToBuilder(string_builder.get(), types::DataType::STRING, 2, 1, 3);

  std::shared_ptr<arrow::Array> time_col;
  std::shared_ptr<arrow::Array> bool_col;
  std::shared_ptr<arrow::Array> string_col;
  EXPECT_OK(time_builder->Finish(&time_col));
  EXPECT_OK(bool_builder->Finish(&bool_col));
  EXPECT_OK(string_builder->Finish(&string_col));

  EXPECT_TRUE(time_col->Equals(types::ToArrow(times_, arrow::default_memory_pool())->Slice(2, 2)));
  EXPECT_TRUE(bool_col->Equals(types::ToArrow(bools_, arrow::default_memory_pool())->Slice(2, 2)));
  EXPECT_TRUE(
      string_col->Equals(types::ToArrow(strings_, arrow::default_memory_pool())->Slice(2, 2)));
}

TEST_P(RecordOrRowBatchTest, GetVariableSizedColumnRowBytes) {
  EXPECT_THAT(rb_->GetVariableSizedColumnRowBytes(2),
              ::testing::ElementsAre(strings_[0].size(), strings_[1].size(), strings_[2].size(),
                                     strings_[3].size()));
}

TEST_P(RecordOrRowBatchTest, RemovePrefix_GetVariableSizedColumnRowBytes) {
  rb_->RemovePrefix(3);
  EXPECT_THAT(rb_->GetVariableSizedColumnRowBytes(2), ::testing::ElementsAre(strings_[3].size()));
}

INSTANTIATE_RECORD_OR_ROW_BATCH_TESTSUITE(RecordOrRowBatch, RecordOrRowBatchTest,
                                          /*include_mixed*/ false);

}  // namespace internal
}  // namespace table_store
}  // namespace px
