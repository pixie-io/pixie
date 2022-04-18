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
#include "src/table_store/table/internal/arrow_array_compactor.h"
#include "src/table_store/table/internal/record_or_row_batch.h"
#include "src/table_store/table/internal/test_utils.h"

namespace px {
namespace table_store {
namespace internal {

class ArrowArrayCompactorTest : public RecordOrRowBatchParamTest {
  void SetUp() override {
    RecordOrRowBatchParamTest::SetUp();
    compactor_ = std::make_unique<ArrowArrayCompactor>(*rel_, arrow::default_memory_pool());
  }

 protected:
  std::unique_ptr<ArrowArrayCompactor> compactor_;
};

TEST_P(ArrowArrayCompactorTest, BasicCompaction) {
  std::vector<types::Time64NSValue> times_rb0 = {1, 2, 3};
  std::vector<types::BoolValue> bools_rb0 = {true, false, true};
  std::vector<types::StringValue> strings_rb0 = {"short", "longer string than first row", "s"};
  std::unique_ptr<RecordOrRowBatch> rb0;
  ColSizes rb0_col_sizes;
  std::tie(rb0, rb0_col_sizes) = MakeRecordOrRowBatch(times_rb0, bools_rb0, strings_rb0);
  size_t rb0_num_rows = times_rb0.size();

  std::vector<types::Time64NSValue> times_rb1 = {9, 10, 20, 25};
  std::vector<types::BoolValue> bools_rb1 = {true, false, true, false};
  std::vector<types::StringValue> strings_rb1 = {
      "one", "very", "long",
      "string. This one. This is the very long one. I'm going to keep going, to make it even "
      "longer, and longer"};
  std::unique_ptr<RecordOrRowBatch> rb1;
  ColSizes rb1_col_sizes;
  std::tie(rb1, rb1_col_sizes) = MakeRecordOrRowBatch(times_rb1, bools_rb1, strings_rb1);
  size_t rb1_num_rows = times_rb1.size();

  auto total_num_rows = rb0_num_rows + rb1_num_rows;
  ColSizes total_col_sizes;
  for (size_t i = 0; i < rel_->NumColumns(); ++i) {
    total_col_sizes.push_back(rb0_col_sizes[i] + rb1_col_sizes[i]);
  }

  ASSERT_OK(compactor_->Reserve(total_num_rows, total_col_sizes));
  compactor_->UnsafeAppendBatchSlice(*rb0, 0, rb0_num_rows);
  compactor_->UnsafeAppendBatchSlice(*rb1, 0, rb1_num_rows);

  ASSERT_OK_AND_ASSIGN(auto out_columns, compactor_->Finish());

  auto out_time_col = out_columns[0];
  auto out_bool_col = out_columns[1];
  auto out_strings_col = out_columns[2];

  EXPECT_TRUE(out_time_col->Slice(0, rb0_num_rows)
                  ->Equals(types::ToArrow(times_rb0, arrow::default_memory_pool())));
  EXPECT_TRUE(out_time_col->Slice(rb0_num_rows, rb1_num_rows)
                  ->Equals(types::ToArrow(times_rb1, arrow::default_memory_pool())));

  EXPECT_TRUE(out_bool_col->Slice(0, rb0_num_rows)
                  ->Equals(types::ToArrow(bools_rb0, arrow::default_memory_pool())));
  EXPECT_TRUE(out_bool_col->Slice(rb0_num_rows, rb1_num_rows)
                  ->Equals(types::ToArrow(bools_rb1, arrow::default_memory_pool())));

  EXPECT_TRUE(out_strings_col->Slice(0, rb0_num_rows)
                  ->Equals(types::ToArrow(strings_rb0, arrow::default_memory_pool())));
  EXPECT_TRUE(out_strings_col->Slice(rb0_num_rows, rb1_num_rows)
                  ->Equals(types::ToArrow(strings_rb1, arrow::default_memory_pool())));
}

TEST_P(ArrowArrayCompactorTest, SlicedCompaction) {
  // Append last row of the first row batch and the first 2 rows of the second
  std::vector<types::Time64NSValue> times_rb0 = {1, 2, 3};
  std::vector<types::BoolValue> bools_rb0 = {true, false, true};
  std::vector<types::StringValue> strings_rb0 = {"short", "longer string than first row", "s"};
  std::unique_ptr<RecordOrRowBatch> rb0;
  ColSizes rb0_col_sizes;
  std::tie(rb0, rb0_col_sizes) = MakeRecordOrRowBatch(times_rb0, bools_rb0, strings_rb0);
  size_t rb0_num_rows = times_rb0.size();

  std::vector<types::Time64NSValue> times_rb1 = {9, 10, 20, 25};
  std::vector<types::BoolValue> bools_rb1 = {true, false, true, false};
  std::vector<types::StringValue> strings_rb1 = {
      "one", "very", "long",
      "string. This one. This is the very long one. I'm going to keep going, to make it even "
      "longer, and longer"};
  std::unique_ptr<RecordOrRowBatch> rb1;
  ColSizes rb1_col_sizes;
  std::tie(rb1, rb1_col_sizes) = MakeRecordOrRowBatch(times_rb1, bools_rb1, strings_rb1);

  // 1 row from the first rb, 2 from the second
  auto total_num_rows = 1 + 2;
  ColSizes total_col_sizes;
  total_col_sizes.push_back(0);
  total_col_sizes.push_back(0);
  total_col_sizes.push_back(strings_rb0.back().size() + strings_rb1[0].size() +
                            strings_rb1[1].size());

  ASSERT_OK(compactor_->Reserve(total_num_rows, total_col_sizes));

  compactor_->UnsafeAppendBatchSlice(*rb0, rb0_num_rows - 1, rb0_num_rows);
  compactor_->UnsafeAppendBatchSlice(*rb1, 0, 2);

  ASSERT_OK_AND_ASSIGN(auto out_columns, compactor_->Finish());

  auto out_time_col = out_columns[0];
  auto out_bool_col = out_columns[1];
  auto out_strings_col = out_columns[2];

  ASSERT_EQ(total_num_rows, out_time_col->length());
  ASSERT_EQ(total_num_rows, out_bool_col->length());
  ASSERT_EQ(total_num_rows, out_strings_col->length());

  EXPECT_TRUE(out_time_col->Equals(
      types::ToArrow(std::vector<types::Time64NSValue>{3, 9, 10}, arrow::default_memory_pool())));

  EXPECT_TRUE(out_bool_col->Equals(types::ToArrow(std::vector<types::BoolValue>{true, true, false},
                                                  arrow::default_memory_pool())));

  EXPECT_TRUE(out_strings_col->Equals(types::ToArrow(
      std::vector<types::StringValue>{"s", "one", "very"}, arrow::default_memory_pool())));
}

INSTANTIATE_RECORD_OR_ROW_BATCH_TESTSUITE(ArrowArrayCompactor, ArrowArrayCompactorTest,
                                          /*include_mixed*/ true);

}  // namespace internal
}  // namespace table_store
}  // namespace px
