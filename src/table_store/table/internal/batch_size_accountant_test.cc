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
#include <utility>

#include "src/table_store/table/internal/batch_size_accountant.h"
#include "src/table_store/table/internal/test_utils.h"

namespace px {
namespace table_store {
namespace internal {

class BatchSizeAccountantTest : public RecordOrRowBatchParamTest {
 protected:
  void SetUp() override {
    RecordOrRowBatchParamTest::SetUp();

    std::vector<types::Time64NSValue> times = {9, 10, 20, 25};
    std::vector<types::BoolValue> bools = {true, false, true, false};
    std::vector<types::StringValue> strings = {
        "ab", "cd", "ef",
        "one row on its own is larger than the compaction size, this row will always be the last "
        "row in a compacted batch. The other 3 rows fit within the size of one compacted batch."};
    for (const auto& s : strings) {
      larger_than_compaction_string_sizes_.push_back(s.size());
    }
    std::tie(larger_than_compaction_rb_, larger_than_compaction_rb_col_sizes_) =
        MakeRecordOrRowBatch(times, bools, strings);
    larger_than_compaction_rb_bytes_ = 4 * sizeof(int64_t) + 4 * sizeof(bool) +
                                       4 * sizeof(uint32_t) +
                                       larger_than_compaction_rb_col_sizes_[2];

    times = {1, 2, 3};
    bools = {true, false, true};
    strings = {"ab", "cd", "ef"};
    for (const auto& s : strings) {
      half_compaction_string_sizes_.push_back(s.size());
    }
    std::tie(half_compaction_rb_, half_compaction_rb_col_sizes_) =
        MakeRecordOrRowBatch(times, bools, strings);

    half_compaction_rb_bytes_ = 3 * sizeof(int64_t) + 3 * sizeof(bool) + 3 * sizeof(uint32_t) +
                                half_compaction_rb_col_sizes_[2];

    accountant_ = BatchSizeAccountant::Create(*rel_, compacted_size_);
  }
  const size_t compacted_size_ = 90;
  std::unique_ptr<BatchSizeAccountant> accountant_;

  std::unique_ptr<RecordOrRowBatch> larger_than_compaction_rb_;
  size_t larger_than_compaction_rb_bytes_;
  ColSizes larger_than_compaction_rb_col_sizes_;
  std::vector<uint64_t> larger_than_compaction_string_sizes_;
  std::unique_ptr<RecordOrRowBatch> half_compaction_rb_;
  size_t half_compaction_rb_bytes_;
  ColSizes half_compaction_rb_col_sizes_;
  std::vector<uint64_t> half_compaction_string_sizes_;

  int64_t string_col_idx_ = 2;
};

TEST_P(BatchSizeAccountantTest, BatchStatsBasic) {
  auto stats = BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(),
                                                   *larger_than_compaction_rb_);
  EXPECT_EQ(4, stats.num_rows);
  EXPECT_EQ(larger_than_compaction_rb_bytes_, stats.bytes);
  EXPECT_THAT(stats.variable_cols_row_bytes,
              ::testing::UnorderedElementsAre(std::pair<int64_t, std::vector<uint64_t>>{
                  string_col_idx_, larger_than_compaction_string_sizes_}));

  stats = BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_);
  EXPECT_EQ(3, stats.num_rows);
  EXPECT_EQ(half_compaction_rb_bytes_, stats.bytes);
  EXPECT_THAT(stats.variable_cols_row_bytes,
              ::testing::ElementsAre(std::pair<int64_t, std::vector<uint64_t>>{
                  string_col_idx_, half_compaction_string_sizes_}));
}

#define HOT_SLICE_MATCHER(start, end, last_slice)                                                \
  ::testing::AllOf(                                                                              \
      ::testing::Field(&BatchSizeAccountant::CompactedBatchSpec::HotSlice::start_row, start),    \
      ::testing::Field(&BatchSizeAccountant::CompactedBatchSpec::HotSlice::end_row, end),        \
      ::testing::Field(&BatchSizeAccountant::CompactedBatchSpec::HotSlice::last_slice_for_batch, \
                       last_slice))

TEST_P(BatchSizeAccountantTest, IndexThenCompact) {
  accountant_->NewHotBatch(
      BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_));
  accountant_->NewHotBatch(BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(),
                                                               *larger_than_compaction_rb_));
  accountant_->NewHotBatch(
      BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_));
  accountant_->NewHotBatch(
      BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_));

  EXPECT_EQ(3 * half_compaction_rb_bytes_ + larger_than_compaction_rb_bytes_,
            accountant_->HotBytes());

  // These four hot batches, should be compacted into 3 cold batches, the first one is the entire
  // first hot batch, plus the first 3 rows of the larger_than_compaction_rb. The second compacted
  // batch should be just the last row of the larger_than_compaction_rb. The last compacted batch
  // should be a concatenation of the 3rd and 4th hot batches.
  ASSERT_TRUE(accountant_->CompactedBatchReady());
  auto compacted_spec = accountant_->GetNextCompactedBatchSpec();
  EXPECT_EQ(6, compacted_spec.num_rows);
  EXPECT_EQ(compacted_size_, compacted_spec.bytes);
  EXPECT_THAT(compacted_spec.hot_slices, ::testing::ElementsAre(HOT_SLICE_MATCHER(0, 3, true),
                                                                HOT_SLICE_MATCHER(0, 3, false)));
  // We expect to remove 3 rows from the second hot batch.
  EXPECT_EQ(3, accountant_->FinishCompactedBatch());

  EXPECT_EQ(larger_than_compaction_rb_bytes_ + half_compaction_rb_bytes_, accountant_->HotBytes());
  EXPECT_EQ(half_compaction_rb_bytes_ + half_compaction_rb_bytes_, accountant_->ColdBytes());

  ASSERT_TRUE(accountant_->CompactedBatchReady());
  compacted_spec = accountant_->GetNextCompactedBatchSpec();
  EXPECT_EQ(1, compacted_spec.num_rows);
  EXPECT_EQ(
      larger_than_compaction_string_sizes_[3] + sizeof(int64_t) + sizeof(bool) + sizeof(int32_t),
      compacted_spec.bytes);
  EXPECT_THAT(compacted_spec.hot_slices, ::testing::ElementsAre(HOT_SLICE_MATCHER(0, 1, true)));
  EXPECT_EQ(0, accountant_->FinishCompactedBatch());

  EXPECT_EQ(half_compaction_rb_bytes_ + half_compaction_rb_bytes_, accountant_->HotBytes());
  EXPECT_EQ(half_compaction_rb_bytes_ + larger_than_compaction_rb_bytes_, accountant_->ColdBytes());

  ASSERT_TRUE(accountant_->CompactedBatchReady());
  compacted_spec = accountant_->GetNextCompactedBatchSpec();
  EXPECT_EQ(6, compacted_spec.num_rows);
  EXPECT_EQ(compacted_size_, compacted_spec.bytes);
  EXPECT_THAT(compacted_spec.hot_slices,
              ::testing::ElementsAre(HOT_SLICE_MATCHER(0, 3, true), HOT_SLICE_MATCHER(0, 3, true)));
  EXPECT_EQ(0, accountant_->FinishCompactedBatch());

  EXPECT_EQ(0, accountant_->HotBytes());
  EXPECT_EQ(3 * half_compaction_rb_bytes_ + larger_than_compaction_rb_bytes_,
            accountant_->ColdBytes());
}

TEST_P(BatchSizeAccountantTest, IndexThenCompactWithExpiry) {
  accountant_->NewHotBatch(
      BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_));
  accountant_->NewHotBatch(BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(),
                                                               *larger_than_compaction_rb_));
  accountant_->NewHotBatch(
      BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_));
  accountant_->NewHotBatch(
      BatchSizeAccountant::CalcBatchStats(accountant_->NonMutableState(), *half_compaction_rb_));

  EXPECT_EQ(3 * half_compaction_rb_bytes_ + larger_than_compaction_rb_bytes_,
            accountant_->HotBytes());

  // These four hot batches, should be compacted into 3 cold batches, the first one is the entire
  // first hot batch, plus the first 3 rows of the larger_than_compaction_rb. The second compacted
  // batch should be just the last row of the larger_than_compaction_rb. The last compacted batch
  // should be a concatenation of the 3rd and 4th hot batches.
  ASSERT_TRUE(accountant_->CompactedBatchReady());
  auto compacted_spec = accountant_->GetNextCompactedBatchSpec();
  EXPECT_EQ(6, compacted_spec.num_rows);
  EXPECT_EQ(compacted_size_, compacted_spec.bytes);
  EXPECT_THAT(compacted_spec.hot_slices, ::testing::ElementsAre(HOT_SLICE_MATCHER(0, 3, true),
                                                                HOT_SLICE_MATCHER(0, 3, false)));
  // We expect to remove 3 rows from the second hot batch.
  EXPECT_EQ(3, accountant_->FinishCompactedBatch());

  EXPECT_EQ(larger_than_compaction_rb_bytes_ + half_compaction_rb_bytes_, accountant_->HotBytes());
  EXPECT_EQ(half_compaction_rb_bytes_ + half_compaction_rb_bytes_, accountant_->ColdBytes());

  // Expire a Hot batch. This should get rid of just the single row.
  accountant_->ExpireHotBatch();
  EXPECT_EQ(2 * half_compaction_rb_bytes_, accountant_->HotBytes());

  ASSERT_TRUE(accountant_->CompactedBatchReady());
  compacted_spec = accountant_->GetNextCompactedBatchSpec();
  EXPECT_EQ(6, compacted_spec.num_rows);
  EXPECT_EQ(compacted_size_, compacted_spec.bytes);
  EXPECT_THAT(compacted_spec.hot_slices,
              ::testing::ElementsAre(HOT_SLICE_MATCHER(0, 3, true), HOT_SLICE_MATCHER(0, 3, true)));
  EXPECT_EQ(0, accountant_->FinishCompactedBatch());

  EXPECT_EQ(0, accountant_->HotBytes());
  EXPECT_EQ(4 * half_compaction_rb_bytes_, accountant_->ColdBytes());

  accountant_->ExpireColdBatch();
  EXPECT_EQ(0, accountant_->HotBytes());
  EXPECT_EQ(2 * half_compaction_rb_bytes_, accountant_->ColdBytes());
}

INSTANTIATE_RECORD_OR_ROW_BATCH_TESTSUITE(BatchSizeAccountant, BatchSizeAccountantTest,
                                          /*include_mixed*/ true);

}  // namespace internal
}  // namespace table_store
}  // namespace px
