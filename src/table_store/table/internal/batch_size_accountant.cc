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
#include <utility>

#include "src/table_store/table/internal/batch_size_accountant.h"

namespace px {
namespace table_store {
namespace internal {

BatchSizeAccountantNonMutableState BatchSizeAccountant::CreateNonMutableState(
    const schema::Relation& rel, size_t compacted_size) {
  BatchSizeAccountantNonMutableState state;
  state.compacted_size = compacted_size;
  state.num_cols = rel.NumColumns();
  state.per_row_fixed_size = 0;
  for (const auto& [col_idx, data_type] : Enumerate(rel.col_types())) {
    // arrow::StringArray's use a 4-byte integer for each entry to store the offset of that entry in
    // the contiguous string buffer, so a StringArray has a fixed cost of `sizeof(int32_t)` in
    // addition to the variable cost to store the string its self.
#define TYPE_CASE(_dt_)                                                           \
  if constexpr (_dt_ == types::DataType::STRING) {                                \
    state.per_row_fixed_size += sizeof(int32_t);                                  \
  } else {                                                                        \
    state.per_row_fixed_size += sizeof(types::DataTypeTraits<_dt_>::native_type); \
  }
    PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE

    if (data_type == types::DataType::STRING) {
      state.variable_cols_indices.push_back(col_idx);
    }
  }
  return state;
}

std::unique_ptr<BatchSizeAccountant> BatchSizeAccountant::Create(const schema::Relation& rel,
                                                                 size_t compacted_size) {
  return std::unique_ptr<BatchSizeAccountant>(
      new BatchSizeAccountant(BatchSizeAccountant::CreateNonMutableState(rel, compacted_size)));
}

BatchSizeAccountant::BatchSizeAccountant(BatchSizeAccountantNonMutableState state)
    : non_mutable_state_(state) {}

BatchSizeAccountant::BatchStats BatchSizeAccountant::CalcBatchStats(
    const BatchSizeAccountantNonMutableState& non_mutable_state, const RecordOrRowBatch& batch) {
  BatchStats stats;
  stats.num_rows = batch.Length();
  stats.bytes = non_mutable_state.per_row_fixed_size * stats.num_rows;

  for (auto col_idx : non_mutable_state.variable_cols_indices) {
    auto rows_bytes = batch.GetVariableSizedColumnRowBytes(col_idx);
    stats.bytes += std::accumulate(rows_bytes.begin(), rows_bytes.end(), 0ULL);
    stats.variable_cols_row_bytes.emplace(col_idx, std::move(rows_bytes));
  }
  return stats;
}

BatchSizeAccountant::CompactedBatchSpec* BatchSizeAccountant::NewCompactedBatchSpec() {
  auto& compacted_spec = compacted_batch_specs_.emplace_back();
  compacted_spec.num_rows = 0;
  compacted_spec.bytes = 0;
  compacted_spec.variable_col_bytes = std::vector<uint64_t>(non_mutable_state_.num_cols, 0);
  return &compacted_spec;
}

void BatchSizeAccountant::NewHotBatch(const BatchStats& batch_stats) {
  CompactedBatchSpec* compacted_spec = nullptr;
  if (!compacted_batch_specs_.empty() &&
      compacted_batch_specs_.back().bytes < non_mutable_state_.compacted_size) {
    compacted_spec = &compacted_batch_specs_.back();
  } else {
    compacted_spec = NewCompactedBatchSpec();
  }

  DCHECK(batch_stats.bytes > 0) << "BatchSizeAccountant does not support 0-sized batches.";
  hot_bytes_ += batch_stats.bytes;

  CompactedBatchSpec::HotSlice curr_hot_slice = {};
  curr_hot_slice.variable_col_bytes = std::vector<uint64_t>(non_mutable_state_.num_cols, 0);

  for (uint64_t row_idx = 0; row_idx < batch_stats.num_rows; ++row_idx) {
    uint64_t row_bytes = non_mutable_state_.per_row_fixed_size;
    for (int64_t col_idx : non_mutable_state_.variable_cols_indices) {
      auto datum_variable_bytes = batch_stats.variable_cols_row_bytes.at(col_idx)[row_idx];
      row_bytes += datum_variable_bytes;
      compacted_spec->variable_col_bytes[col_idx] += datum_variable_bytes;
      curr_hot_slice.variable_col_bytes[col_idx] += datum_variable_bytes;
    }
    curr_hot_slice.end_row++;
    compacted_spec->num_rows++;
    compacted_spec->bytes += row_bytes;
    curr_hot_slice.bytes += row_bytes;
    if (compacted_spec->bytes >= non_mutable_state_.compacted_size) {
      curr_hot_slice.last_slice_for_batch = (row_idx == batch_stats.num_rows - 1);
      compacted_spec->hot_slices.push_back(std::move(curr_hot_slice));
      curr_hot_slice = CompactedBatchSpec::HotSlice{};
      curr_hot_slice.start_row = row_idx + 1;
      curr_hot_slice.end_row = row_idx + 1;
      curr_hot_slice.variable_col_bytes = std::vector<uint64_t>(non_mutable_state_.num_cols, 0);
      compacted_spec = NewCompactedBatchSpec();
    }
  }

  // If we added any rows to the current hot slice, push it to the compacted spec.
  if (curr_hot_slice.start_row != curr_hot_slice.end_row) {
    curr_hot_slice.last_slice_for_batch = true;
    compacted_spec->hot_slices.push_back(std::move(curr_hot_slice));
  }
}

void BatchSizeAccountant::ExpireHotBatch() {
  // Walk the hot slices, erasing each one, until a slice with last_slice_for_batch is reached. Also
  // erase CompactedBatchSpec's that no longer have any hot slices.
  auto spec_it = compacted_batch_specs_.begin();
  while (spec_it != compacted_batch_specs_.end()) {
    auto slice_it = spec_it->hot_slices.begin();
    while (slice_it != spec_it->hot_slices.end()) {
      spec_it->num_rows -= (slice_it->end_row - slice_it->start_row);
      spec_it->bytes -= slice_it->bytes;
      hot_bytes_ -= slice_it->bytes;
      for (const auto& [col_idx, col_bytes] : Enumerate(slice_it->variable_col_bytes)) {
        spec_it->variable_col_bytes[col_idx] -= col_bytes;
      }
      bool last_slice = slice_it->last_slice_for_batch;
      slice_it = spec_it->hot_slices.erase(slice_it);
      if (last_slice) {
        if (spec_it->hot_slices.empty()) {
          compacted_batch_specs_.erase(spec_it);
        }
        return;
      }
    }
    spec_it = compacted_batch_specs_.erase(spec_it);
  }
}

void BatchSizeAccountant::ExpireColdBatch() {
  cold_bytes_ -= cold_batch_bytes_.front();
  cold_batch_bytes_.pop_front();
}

bool BatchSizeAccountant::CompactedBatchReady() const {
  return !compacted_batch_specs_.empty() &&
         (compacted_batch_specs_.front().bytes >= non_mutable_state_.compacted_size);
}

const BatchSizeAccountant::CompactedBatchSpec& BatchSizeAccountant::GetNextCompactedBatchSpec()
    const {
  DCHECK(CompactedBatchReady());
  return compacted_batch_specs_.front();
}

uint64_t BatchSizeAccountant::FinishCompactedBatch() {
  DCHECK(CompactedBatchReady());
  auto spec = std::move(compacted_batch_specs_.front());
  compacted_batch_specs_.pop_front();

  hot_bytes_ -= spec.bytes;
  cold_bytes_ += spec.bytes;
  cold_batch_bytes_.push_back(spec.bytes);

  if (spec.hot_slices.back().last_slice_for_batch) {
    // If the last slice in the compacted batch was the last slice for the corresponding hot batch,
    // then there is no need to remove rows from the front of the hot store.
    return 0;
  }
  // If the last slice in the compacted batch was not the last slice for the corresponding hot
  // batch, then rows need to be removed from the hot batch to avoid duplication of rows between the
  // hot and cold stores.

  // There should be another hot slice in another compacted batch spec if the end of the just popped
  // spec was not the end of the corresponding hot batch.
  DCHECK(!compacted_batch_specs_.empty());
  DCHECK(!compacted_batch_specs_.front().hot_slices.empty());

  auto& next_spec = compacted_batch_specs_.front();
  auto rows_to_remove = next_spec.hot_slices.front().start_row;

  // Update start and end rows for each slice until we reach a slice with last_slice_for_batch ==
  // true.
  for (auto& spec : compacted_batch_specs_) {
    for (auto& slice : spec.hot_slices) {
      slice.start_row -= rows_to_remove;
      slice.end_row -= rows_to_remove;

      if (slice.last_slice_for_batch) {
        return rows_to_remove;
      }
    }
  }
  // There should always be a slice with last_slice_for_batch == true.
  DCHECK(false);
  return rows_to_remove;
}

uint64_t BatchSizeAccountant::HotBytes() const { return hot_bytes_; }

uint64_t BatchSizeAccountant::ColdBytes() const { return cold_bytes_; }

const BatchSizeAccountantNonMutableState& BatchSizeAccountant::NonMutableState() const {
  return non_mutable_state_;
}

}  // namespace internal
}  // namespace table_store
}  // namespace px
