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

#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "src/common/base/status.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/table/internal/types.h"

namespace px {
namespace table_store {
namespace internal {

using HotBatch = RecordOrRowBatch;

inline bool RowIDIntervalComparator(const RowIDInterval& interval, RowID val) {
  return interval.second < val;
}

inline bool TimeIntervalComparatorLowerBound(const TimeInterval& interval, Time val) {
  return interval.second < val;
}

inline bool TimeIntervalComparatorUpperBound(Time val, const TimeInterval& interval) {
  return val < interval.second;
}

template <bool always_false = false>
void constexpr_else_static_assert_false() {
  static_assert(always_false, "constexpr else block reached");
}

/**
 * StoreWithRowTimeAccounting stores a deque of batches (hot or cold) and keeps track of the first
 * and last unique RowID's for each batch, as well as the first and last times for each batch (if
 * there is a time column in the table). The template parameter specifies whether this is the Hot or
 * Cold store. Since the logic between the Hot and Cold stores is roughly identical, this class
 * deduplicates that logic while allowing the explicit batch accesses to use the correct Hot or Cold
 * batch methods.
 *
 * Times are used to find row batch's within a given time
 * range. RowIDs are used in case table compaction occurs during query execution. Since the size of
 * the batches changes when they are compacted from the hot store to the cold store, the unique
 * RowIDs are necessary to ensure that the query doesn't receive duplicate rows if the rows have
 * the same timestamp.
 */
template <StoreType TStoreType>
class StoreWithRowTimeAccounting {
  using TBatch = typename StoreTypeTraits<TStoreType>::batch_type;

 public:
  StoreWithRowTimeAccounting(const schema::Relation& rel, int64_t time_col_idx)
      : rel_(rel), time_col_idx_(time_col_idx) {}

  /**
   * GetNextRowBatch returns the next row batch in this store after the given unique row id.
   * @param last_read_row_id, pointer to the unique RowID of the last read row. The outputted batch
   * should include only rows with a RowID greater than this RowID. After determining the output
   * batch, this pointer is updated to point to the RowID of the last row in the outputted batch.
   * @param hints, pointer to a BatchHints object (usually from a Table::Cursor), that provides a
   * hint to the store about which batch should be next. If the hint is correct, no searching for
   * the right batch is required, otherwise searching is performed as usual. This is purely an
   * optimization and passing a `nullptr` for hints is accepted.
   * @param stop_row_id, an optional unique RowID to stop the batch at. If provided, the batch will
   * be sliced such that no rows are included with `RowID >= stop_row_id.value()`.
   * @param cols, a vector of column indices to include in the outputted row batch.
   * @return a unique_ptr to the RowBatch or nullptr if there are no more rows in this store that
   * match the parameters above. On error returns a Status.
   */
  StatusOr<std::unique_ptr<schema::RowBatch>> GetNextRowBatch(
      RowID* last_read_row_id, BatchHints* hints, std::optional<RowID> stop_row_id,
      const std::vector<int64_t>& cols) const {
    auto start_row_id = *last_read_row_id + 1;
    if (batches_.empty() || start_row_id < FirstRowID() || start_row_id > LastRowID()) {
      return std::unique_ptr<schema::RowBatch>(nullptr);
    }
    if (DCHECK_IS_ON() && stop_row_id.has_value()) {
      DCHECK_LT(start_row_id, stop_row_id.value());
    }

    BatchID batch_id;
    if (hints != nullptr && BatchHintValid(*hints, start_row_id)) {
      batch_id = hints->batch_id;
    } else {
      batch_id = FindBatchIDFromRowID(start_row_id);
    }

    const auto& batch = GetBatchFromBatchID(batch_id);
    RowID batch_first_row_id = BatchFirstRowID(batch_id);
    RowID batch_last_row_id = BatchLastRowID(batch_id);
    size_t row_offset = start_row_id - batch_first_row_id;
    size_t batch_size = batch_last_row_id - start_row_id + 1;
    if (stop_row_id.has_value() && batch_last_row_id >= stop_row_id.value()) {
      // Reduce batch size if the batch extends past the given stop row.
      batch_size -= (batch_last_row_id - stop_row_id.value()) + 1;
    }

    // Get column types for row descriptor.
    std::vector<types::DataType> col_types;
    for (int64_t col_idx : cols) {
      DCHECK(static_cast<size_t>(col_idx) < rel_.NumColumns());
      col_types.push_back(rel_.col_types()[col_idx]);
    }
    auto output_rb =
        std::make_unique<schema::RowBatch>(schema::RowDescriptor(col_types), batch_size);
    PX_RETURN_IF_ERROR(
        AddBatchSliceToRowBatch(batch, row_offset, batch_size, cols, output_rb.get()));

    // Update the ptr to the last read row.
    *last_read_row_id = start_row_id + batch_size - 1;

    // Set hints to point to the next batch in the current store. It's fine if that batch doesn't
    // exist, as the next call will ignore the hints if that's the case.
    hints->batch_id = batch_id + 1;
    hints->hint_type = TStoreType;
    return output_rb;
  }

  /**
   * Size returns the number of batches in this store.
   * @return number of batches.
   */
  size_t Size() const { return batches_.size(); }

  /**
   * front gets a reference to the first batch in the store.
   * @return reference to the first batch in the store.
   */
  TBatch& front() {
    DCHECK(!batches_.empty());
    return batches_.front();
  }

  /**
   * PopFront removes the first batch in the store, and returns an rvalue reference to it.
   * @return rvalue reference to the removed batch.
   */
  TBatch&& PopFront() {
    DCHECK(!batches_.empty());
    first_batch_id_++;

    row_ids_.pop_front();
    if (time_col_idx_ != -1) times_.pop_front();

    auto&& front = std::move(batches_.front());
    batches_.pop_front();
    return std::move(front);
  }

  /**
   * EmplaceBack creates a batch at the back of the store with the given args, and updates the
   * accounting such that the first RowID of the batch is the given first_row_id.
   * @param first_row_id, unique RowID to use as the first RowID for the emplaced batch.
   * @return lvalue reference to the emplaced batch.
   */
  template <typename... Args>
  TBatch& EmplaceBack(RowID first_row_id, Args... args) {
    auto& batch = batches_.emplace_back(std::forward<Args>(args)...);

    row_ids_.emplace_back(first_row_id, first_row_id + BatchLength(batch) - 1);
    if (time_col_idx_ != -1) {
      auto first_time = GetTimeValue(batch, 0);
      auto last_time = GetTimeValue(batch, BatchLength(batch) - 1);
      times_.emplace_back(first_time, last_time);
    }
    return batch;
  }

  /**
   * FirstRowID returns the RowID of the first row in the store.
   * @return RowID of the first row in the store.
   */
  RowID FirstRowID() const {
    DCHECK(!batches_.empty());
    return row_ids_.front().first;
  }

  /**
   * LastRowID returns the RowID of the last row in the store.
   * @return RowID of the last row in the store.
   */
  RowID LastRowID() const {
    DCHECK(!batches_.empty());
    return row_ids_.back().second;
  }

  /**
   * FindRowIDFromTimeFirstGreaterThanOrEqual returns the RowID of the first row in the store with
   * time greater than or equal to the given time, or returns std::nullopt if no such row exists.
   * @param time, time to search for.
   * @return RowID of the first row in the store with time greater than or equal to the given time
   * (or std::nullopt if no row is found).
   */
  std::optional<RowID> FindRowIDFromTimeFirstGreaterThanOrEqual(Time time) const {
    if (time_col_idx_ == -1) {
      return std::nullopt;
    }
    auto it =
        std::lower_bound(times_.begin(), times_.end(), time, TimeIntervalComparatorLowerBound);
    if (it == times_.end()) {
      return std::nullopt;
    }
    size_t batch_index = std::distance(times_.begin(), it);
    auto row_offset = FindTimeFirstGreaterThanOrEqual(batches_[batch_index], time);
    return row_ids_[batch_index].first + row_offset;
  }

  /**
   * FindRowIDFromTimeGreaterThan returns the RowID of the first row in the store with
   * time greater than the given time, or returns std::nullopt if no such row exists.
   * @param time, time to search for.
   * @return RowID of the first row in the store with time greater than given time
   * (or std::nullopt if no row is found).
   */
  std::optional<RowID> FindRowIDFromTimeFirstGreaterThan(Time time) const {
    if (time_col_idx_ == -1) {
      return std::nullopt;
    }
    auto it =
        std::upper_bound(times_.begin(), times_.end(), time, TimeIntervalComparatorUpperBound);
    if (it == times_.end()) {
      return std::nullopt;
    }
    size_t batch_index = std::distance(times_.begin(), it);
    auto row_offset = FindTimeFirstGreaterThan(batches_[batch_index], time);
    return row_ids_[batch_index].first + row_offset;
  }

  /**
   * RemovePrefix removes the given number of rows from the first batch in the store. This method is
   * only valid for the `Hot` store, and fails to compile if called on the `Cold` store. Note that
   * no reallocation or copies occur when removing prefix, instead the HotBatch representation
   * maintains a row offset internally that is updated when remove prefix is called on it.
   * @param num_rows, number of rows to remove.
   */
  void RemovePrefix(size_t num_rows) {
    DCHECK(!batches_.empty());

    if constexpr (std::is_same_v<TBatch, HotBatch>) {
      batches_.front().RemovePrefix(num_rows);
    } else {
      constexpr_else_static_assert_false();
    }

    row_ids_.front().first += num_rows;
    if (time_col_idx_ != -1) {
      times_.front().first = GetTimeValue(batches_.front(), 0);
    }
  }

  /**
   * MinTime returns the minimum time in the store. Since the store is assumed to be time-sorted,
   * this is equivalent to returning the time of the first row in the store.
   * @return minimum time in the store, or -1 if there are no rows in the store or there is no time
   * column.
   */
  int64_t MinTime() const {
    if (time_col_idx_ == -1 || times_.empty()) {
      return -1;
    }
    return times_.front().first;
  }
  /**
   * MaxTime returns the maximum time in the store. Since the store is assumed to be time-sorted,
   * this is equivalent to returning the time of the last row in the store.
   * @return maximum time in the store, or -1 if there are no rows in the store or there is no time
   * column.
   */
  int64_t MaxTime() const {
    if (time_col_idx_ == -1 || times_.empty()) {
      return -1;
    }
    return times_.back().second;
  }

 private:
  BatchID LastBatchID() const { return first_batch_id_ + batches_.size() - 1; }

  RowID BatchFirstRowID(BatchID batch_id) const {
    DCHECK_GE(batch_id, first_batch_id_);
    DCHECK_LT(batch_id, first_batch_id_ + static_cast<int64_t>(batches_.size()));
    return row_ids_[batch_id - first_batch_id_].first;
  }

  RowID BatchLastRowID(BatchID batch_id) const {
    DCHECK_GE(batch_id, first_batch_id_);
    DCHECK_LT(batch_id, first_batch_id_ + static_cast<int64_t>(batches_.size()));
    return row_ids_[batch_id - first_batch_id_].second;
  }

  TBatch& GetBatchFromBatchID(BatchID batch_id) {
    DCHECK_GE(batch_id, first_batch_id_);
    DCHECK_LT(batch_id, first_batch_id_ + static_cast<int64_t>(batches_.size()));
    return batches_[batch_id - first_batch_id_];
  }

  const TBatch& GetBatchFromBatchID(BatchID batch_id) const {
    DCHECK_GE(batch_id, first_batch_id_);
    DCHECK_LT(batch_id, first_batch_id_ + static_cast<int64_t>(batches_.size()));
    return batches_[batch_id - first_batch_id_];
  }

  bool BatchHintValid(const BatchHints& hints, RowID row_id) const {
    if (hints.hint_type != TStoreType) {
      return false;
    }
    auto hint_batch_id = hints.batch_id;
    if (hint_batch_id < first_batch_id_ || hint_batch_id > LastBatchID()) {
      return false;
    }
    return BatchFirstRowID(hint_batch_id) <= row_id && row_id <= BatchLastRowID(hint_batch_id);
  }

  BatchID FindBatchIDFromRowID(RowID row_id) const {
    auto it = std::lower_bound(row_ids_.begin(), row_ids_.end(), row_id, RowIDIntervalComparator);
    DCHECK(it != row_ids_.end());
    return first_batch_id_ + std::distance(row_ids_.begin(), it);
  }

  size_t BatchLength(const TBatch& batch) const {
    if constexpr (std::is_same_v<ColdBatch, TBatch>) {
      return batch[0]->length();
    } else if constexpr (std::is_same_v<HotBatch, TBatch>) {
      return batch.Length();
    } else {
      constexpr_else_static_assert_false();
    }
  }

  size_t FindTimeFirstGreaterThanOrEqual(const TBatch& batch, Time time) const {
    if constexpr (std::is_same_v<TBatch, ColdBatch>) {
      return types::SearchArrowArrayGreaterThanOrEqual<types::DataType::TIME64NS>(
          batch[time_col_idx_].get(), time);
    } else if constexpr (std::is_same_v<TBatch, HotBatch>) {
      return batch.FindTimeFirstGreaterThanOrEqual(time_col_idx_, time);
    } else {
      constexpr_else_static_assert_false();
    }
  }

  size_t FindTimeFirstGreaterThan(const TBatch& batch, Time time) const {
    if constexpr (std::is_same_v<TBatch, ColdBatch>) {
      return types::SearchArrowArrayLessThanOrEqual<types::DataType::TIME64NS>(
                 batch[time_col_idx_].get(), time) +
             1;
    } else if constexpr (std::is_same_v<TBatch, HotBatch>) {
      return batch.FindTimeFirstGreaterThan(time_col_idx_, time);
    } else {
      constexpr_else_static_assert_false();
    }
  }

  Time GetTimeValue(const TBatch& batch, int64_t row_idx) const {
    if constexpr (std::is_same_v<TBatch, ColdBatch>) {
      return types::GetValueFromArrowArray<types::DataType::TIME64NS>(batch[time_col_idx_].get(),
                                                                      row_idx);
    } else if constexpr (std::is_same_v<TBatch, HotBatch>) {
      return batch.GetTimeValue(time_col_idx_, row_idx);
    } else {
      constexpr_else_static_assert_false();
    }
  }

  Status AddBatchSliceToRowBatch(const TBatch& batch, size_t row_offset, size_t batch_size,
                                 const std::vector<int64_t>& cols,
                                 schema::RowBatch* output_rb) const {
    if constexpr (std::is_same_v<TBatch, ColdBatch>) {
      for (auto col_idx : cols) {
        auto arr = batch[col_idx]->Slice(row_offset, batch_size);
        PX_RETURN_IF_ERROR(output_rb->AddColumn(arr));
      }
      return Status::OK();
    } else if constexpr (std::is_same_v<TBatch, HotBatch>) {
      return batch.AddBatchSliceToRowBatch(row_offset, batch_size, cols, output_rb);
    } else {
      constexpr_else_static_assert_false();
    }
  }

  BatchID first_batch_id_ = 0;
  const schema::Relation& rel_;
  const int64_t time_col_idx_;
  std::deque<TBatch> batches_;
  std::deque<RowIDInterval> row_ids_;
  std::deque<TimeInterval> times_;
};

}  // namespace internal
}  // namespace table_store
}  // namespace px
