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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/table/table.h"

// Note: this value is not used in most cases.
// Check out PL_TABLE_STORE_DATA_LIMIT_MB to configure the table store size.
DEFINE_int32(table_store_table_size_limit,
             gflags::Int32FromEnv("PL_TABLE_STORE_TABLE_SIZE_LIMIT", 1024 * 1024 * 64),
             "The maximal size a table allows. When the size grows beyond this limit, "
             "old data will be discarded.");

namespace px {
namespace table_store {

ArrowArrayCompactor::ArrowArrayCompactor(const schema::Relation& rel, arrow::MemoryPool* mem_pool)
    : output_columns_(rel.NumColumns()), column_types_(rel.col_types()) {
  for (auto col_type : column_types_) {
    builders_.push_back(types::MakeArrowBuilder(col_type, mem_pool));
  }
}

template <>
Status ArrowArrayCompactor::AppendColumnTyped<types::DataType::STRING>(
    int64_t col_idx, std::shared_ptr<arrow::Array> arr) {
  auto builder_untyped = builders_[col_idx].get();
  auto builder = static_cast<arrow::StringBuilder*>(builder_untyped);
  auto typed_arr = std::static_pointer_cast<arrow::StringArray>(arr);
  auto size = types::GetArrowArrayBytes<types::DataType::STRING>(typed_arr.get());
  PL_RETURN_IF_ERROR(builder->Reserve(typed_arr->length()));
  PL_RETURN_IF_ERROR(builder->ReserveData(size));
  for (int i = 0; i < typed_arr->length(); ++i) {
    builder->UnsafeAppend(typed_arr->GetString(i));
  }
  bytes_ += size;
  return Status::OK();
}

Status ArrowArrayCompactor::AppendColumn(int64_t col_idx, std::shared_ptr<arrow::Array> arr) {
#define TYPE_CASE(_dt_) PL_RETURN_IF_ERROR(AppendColumnTyped<_dt_>(col_idx, arr));
  PL_SWITCH_FOREACH_DATATYPE(column_types_[col_idx], TYPE_CASE);
#undef TYPE_CASE
  return Status::OK();
}

Status ArrowArrayCompactor::Finish() {
  for (const auto& [col_idx, col_type] : Enumerate(column_types_)) {
#define TYPE_CASE(_dt_) PL_RETURN_IF_ERROR(FinishTyped<_dt_>(col_idx));
    PL_SWITCH_FOREACH_DATATYPE(col_type, TYPE_CASE);
#undef TYPE_CASE
  }
  return Status::OK();
}

Table::Table(std::string_view table_name, const schema::Relation& relation, size_t max_table_size,
             size_t min_cold_batch_size)
    : metrics_(&(GetMetricsRegistry()), std::string(table_name)),
      rel_(relation),
      max_table_size_(max_table_size),
      min_cold_batch_size_(min_cold_batch_size),
      ring_capacity_(max_table_size / min_cold_batch_size) {
  absl::MutexLock gen_lock(&generation_lock_);
  absl::MutexLock cold_lock(&cold_lock_);
  absl::MutexLock hot_lock(&hot_lock_);
  for (const auto& [i, col_name] : Enumerate(rel_.col_names())) {
    if (col_name == "time_" && rel_.GetColumnType(i) == types::DataType::TIME64NS) {
      time_col_idx_ = i;
    }
    cold_column_buffers_.emplace_back(ring_capacity_);
  }
}

Status Table::ToProto(table_store::schemapb::Table* table_proto) const {
  CHECK(table_proto != nullptr);
  std::vector<int64_t> col_selector;
  for (int64_t i = 0; i < static_cast<int64_t>(rel_.NumColumns()); i++) {
    col_selector.push_back(i);
  }

  for (auto it = FirstBatch(); it.IsValid(); it = NextBatch(it)) {
    PL_ASSIGN_OR_RETURN(auto cur_rb,
                        GetRowBatchSlice(it, col_selector, arrow::default_memory_pool()));
    auto next = NextBatch(it);
    auto eos = !next.IsValid();
    cur_rb->set_eow(eos);
    cur_rb->set_eos(eos);
    PL_RETURN_IF_ERROR(cur_rb->ToProto(table_proto->add_row_batches()));
  }

  PL_RETURN_IF_ERROR(rel_.ToProto(table_proto->mutable_relation()));
  return Status::OK();
}

StatusOr<std::unique_ptr<schema::RowBatch>> Table::GetRowBatchSlice(
    const BatchSlice& slice, const std::vector<int64_t>& cols, arrow::MemoryPool* mem_pool) const {
  if (!slice.IsValid())
    return error::InvalidArgument("GetRowBatchSlice called on invalid BatchSlice");
  // Get column types for row descriptor.
  std::vector<types::DataType> rb_types;
  for (int64_t col_idx : cols) {
    DCHECK(static_cast<size_t>(col_idx) < rel_.NumColumns());
    rb_types.push_back(rel_.col_types()[col_idx]);
  }

  auto batch_size = slice.Size();
  auto output_rb = std::make_unique<schema::RowBatch>(schema::RowDescriptor(rb_types), batch_size);
  PL_RETURN_IF_ERROR(AddBatchSliceToRowBatch(slice, cols, output_rb.get(), mem_pool));
  return output_rb;
}

Status Table::ExpireRowBatches(int64_t row_batch_size) {
  if (row_batch_size > max_table_size_) {
    return error::InvalidArgument("RowBatch size ($0) is bigger than maximum table size ($1).",
                                  row_batch_size, max_table_size_);
  }
  int64_t bytes;
  {
    absl::base_internal::SpinLockHolder lock(&stats_lock_);
    bytes = cold_bytes_ + hot_bytes_;
  }
  while (bytes + row_batch_size > max_table_size_) {
    PL_RETURN_IF_ERROR(ExpireBatch());
    {
      absl::base_internal::SpinLockHolder lock(&stats_lock_);
      batches_expired_++;
      bytes = cold_bytes_ + hot_bytes_;
    }
  }
  return Status::OK();
}

Status Table::WriteRowBatch(const schema::RowBatch& rb) {
  // Don't write empty row batches.
  if (rb.num_columns() == 0 || rb.ColumnAt(0)->length() == 0) {
    return Status::OK();
  }
  int64_t rb_bytes = 0;
  // Check for matching types
  auto received_num_columns = rb.num_columns();
  auto expected_num_columns = rel_.NumColumns();
  CHECK_EQ(expected_num_columns, received_num_columns)
      << absl::StrFormat("Table schema mismatch: expected=%u received=%u)", expected_num_columns,
                         received_num_columns);

  for (int i = 0; i < rb.num_columns(); ++i) {
    auto received_type = rb.desc().type(i);
    auto expected_type = rel_.col_types().at(i);
    DCHECK_EQ(expected_type, received_type)
        << absl::StrFormat("Type mismatch [column=%u]: expected=%s received=%s", i,
                           ToString(expected_type), ToString(received_type));
#define TYPE_CASE(_dt_) rb_bytes += types::GetArrowArrayBytes<_dt_>(rb.columns().at(i).get());
    PL_SWITCH_FOREACH_DATATYPE(received_type, TYPE_CASE);
#undef TYPE_CASE
  }

  PL_RETURN_IF_ERROR(ExpireRowBatches(rb_bytes));
  PL_RETURN_IF_ERROR(WriteHot(rb));
  absl::base_internal::SpinLockHolder lock(&stats_lock_);
  hot_bytes_ += rb_bytes;
  ++batches_added_;
  return Status::OK();
}

Status Table::TransferRecordBatch(
    std::unique_ptr<px::types::ColumnWrapperRecordBatch> record_batch) {
  // Don't transfer over empty row batches.
  if (record_batch->empty() || record_batch->at(0)->Size() == 0) {
    return Status::OK();
  }

  // Check for matching types
  auto received_num_columns = record_batch->size();
  auto expected_num_columns = rel_.NumColumns();
  CHECK_EQ(expected_num_columns, received_num_columns)
      << absl::StrFormat("Table schema mismatch: expected=%u received=%u)", expected_num_columns,
                         received_num_columns);

  uint32_t i = 0;
  int64_t rb_bytes = 0;
  for (const auto& col : *record_batch) {
    auto received_type = col->data_type();
    auto expected_type = rel_.col_types().at(i);
    DCHECK_EQ(expected_type, received_type)
        << absl::StrFormat("Type mismatch [column=%u]: expected=%s received=%s", i,
                           ToString(expected_type), ToString(received_type));
    rb_bytes += col->Bytes();
    ++i;
  }

  PL_RETURN_IF_ERROR(ExpireRowBatches(rb_bytes));
  PL_RETURN_IF_ERROR(WriteHot(std::move(record_batch)));

  absl::base_internal::SpinLockHolder lock(&stats_lock_);
  hot_bytes_ += rb_bytes;
  ++batches_added_;

  return Status::OK();
}

static inline bool IntervalComparatorLowerBound(const std::pair<int64_t, int64_t> interval,
                                                int64_t val) {
  return interval.second < val;
}

static inline bool IntervalComparatorUpperBound(int64_t val,
                                                const std::pair<int64_t, int64_t> interval) {
  return val < interval.first;
}

StatusOr<BatchSlice> Table::FindBatchSliceGreaterThanOrEqual(int64_t time,
                                                             arrow::MemoryPool* mem_pool) const {
  if (time_col_idx_ == -1) {
    return error::InvalidArgument(
        "Cannot call FindBatchSliceGreaterThanOrEqual on table without a time column.");
  }
  absl::MutexLock gen_lock(&generation_lock_);
  {
    absl::MutexLock cold_lock(&cold_lock_);
    auto it =
        std::lower_bound(cold_time_.begin(), cold_time_.end(), time, IntervalComparatorLowerBound);
    if (it != cold_time_.end()) {
      auto index = std::distance(cold_time_.begin(), it);
      auto ring_index = RingIndexUnlocked(index);
      auto time_col = cold_column_buffers_[time_col_idx_][ring_index];
      auto row_offset = types::SearchArrowArrayGreaterThanOrEqual<types::DataType::TIME64NS>(
          time_col.get(), time);
      auto row_ids = cold_row_ids_[index];
      return BatchSlice::Cold(ring_index, row_offset, time_col->length() - 1, generation_,
                              row_ids.first + row_offset, row_ids.second);
    }
  }
  // If the time wasn't found in the cold batches, we look in the hot batches.
  absl::MutexLock hot_lock(&hot_lock_);
  auto it =
      std::lower_bound(hot_time_.begin(), hot_time_.end(), time, IntervalComparatorLowerBound);
  if (it == hot_time_.end()) {
    return BatchSlice::Invalid();
  }
  auto index = std::distance(hot_time_.begin(), it);

  ArrowArrayPtr time_col;
  if (std::holds_alternative<RecordBatchWithCache>(hot_batches_[index])) {
    auto record_batch_ptr = std::get_if<RecordBatchWithCache>(&hot_batches_[index]);
    time_col = GetHotColumnUnlocked(record_batch_ptr, time_col_idx_, mem_pool);
  } else {
    auto row_batch = std::get<schema::RowBatch>(hot_batches_[index]);
    time_col = row_batch.columns().at(time_col_idx_);
  }

  auto row_offset =
      types::SearchArrowArrayGreaterThanOrEqual<types::DataType::TIME64NS>(time_col.get(), time);
  auto row_ids = hot_row_ids_[index];
  return BatchSlice::Hot(index, row_offset, time_col->length() - 1, generation_,
                         row_ids.first + row_offset, row_ids.second);
}

StatusOr<Table::StopPosition> Table::FindStopPositionForTime(int64_t time,
                                                             arrow::MemoryPool* mem_pool) const {
  if (time_col_idx_ == -1) {
    return error::InvalidArgument(
        "Cannot call FindStopPositionForTime on table without a time column.");
  }
  auto stop = FindStopTime(time, mem_pool);
  if (stop == -1) {
    // If all the data is after the stop time then we return the first unique row identifier in the
    // table, which will cause no results to be returned.
    return FirstBatch().uniq_row_start_idx;
  }
  return stop + 1;
}

schema::Relation Table::GetRelation() const { return rel_; }

TableStats Table::GetTableStats() const {
  TableStats info;
  auto num_batches = NumBatches();
  int64_t min_time = -1;
  {
    absl::MutexLock cold_lock(&cold_lock_);
    if (time_col_idx_ != -1 && !cold_time_.empty()) {
      min_time = cold_time_[0].first;
    } else {
      absl::MutexLock hot_lock(&hot_lock_);
      if (time_col_idx_ != -1 && !hot_time_.empty()) {
        min_time = hot_time_[0].first;
      }
    }
  }
  absl::base_internal::SpinLockHolder lock(&stats_lock_);

  info.batches_added = batches_added_;
  info.batches_expired = batches_expired_;
  info.num_batches = num_batches;
  info.bytes = hot_bytes_ + cold_bytes_;
  info.cold_bytes = cold_bytes_;
  info.compacted_batches = compacted_batches_;
  info.max_table_size = max_table_size_;
  info.min_time = min_time;

  return info;
}

Status Table::UpdateTimeRowIndices(types::ColumnWrapperRecordBatch* record_batch) {
  auto batch_length = record_batch->at(0)->Size();
  DCHECK_GT(batch_length, 0);
  if (time_col_idx_ != -1) {
    auto first_time = record_batch->at(time_col_idx_)->Get<types::Time64NSValue>(0);
    auto last_time = record_batch->at(time_col_idx_)->Get<types::Time64NSValue>(batch_length - 1);
    hot_time_.emplace_back(first_time.val, last_time.val);
  }
  auto first_row_id = next_row_id_;
  next_row_id_ += batch_length;
  hot_row_ids_.emplace_back(first_row_id, next_row_id_ - 1);
  return Status::OK();
}

Status Table::WriteHot(RecordBatchPtr record_batch) {
  absl::MutexLock hot_lock(&hot_lock_);
  PL_RETURN_IF_ERROR(UpdateTimeRowIndices(record_batch.get()));
  auto rb = RecordBatchWithCache{
      std::move(record_batch),
      std::vector<ArrowArrayPtr>(rel_.NumColumns()),
      std::vector<bool>(rel_.NumColumns(), false),
  };
  hot_batches_.emplace_back(std::move(rb));
  return Status::OK();
}

Status Table::UpdateTimeRowIndices(const schema::RowBatch& rb) {
  auto batch_length = rb.ColumnAt(0)->length();
  DCHECK_GT(batch_length, 0);
  if (time_col_idx_ != -1) {
    auto time_col = rb.ColumnAt(time_col_idx_);
    auto first_time = types::GetValueFromArrowArray<types::DataType::TIME64NS>(time_col.get(), 0);
    auto last_time =
        types::GetValueFromArrowArray<types::DataType::TIME64NS>(time_col.get(), batch_length - 1);
    hot_time_.emplace_back(first_time, last_time);
  }
  auto first_row_id = next_row_id_;
  next_row_id_ += batch_length;
  hot_row_ids_.emplace_back(first_row_id, next_row_id_ - 1);
  return Status::OK();
}

Status Table::WriteHot(const schema::RowBatch& rb) {
  absl::MutexLock hot_lock(&hot_lock_);
  PL_RETURN_IF_ERROR(UpdateTimeRowIndices(rb));
  hot_batches_.emplace_back(rb);
  return Status::OK();
}

Status Table::CompactSingleBatch(arrow::MemoryPool* mem_pool) {
  ArrowArrayCompactor builder(rel_, mem_pool);
  int64_t first_time = -1;
  int64_t last_time = -1;
  int64_t first_row_id = -1;
  int64_t last_row_id = -1;
  absl::MutexLock gen_lock(&generation_lock_);
  // We first get the necessary batches to compact from hot storage. Then we compact those batches
  // into one batch. Then we push that batch into cold storage.
  {
    absl::MutexLock hot_lock(&hot_lock_);
    for (auto it = hot_batches_.begin(); it != hot_batches_.end();) {
      if (builder.Size() >= min_cold_batch_size_) {
        break;
      }
      std::vector<std::shared_ptr<arrow::Array>> column_arrays;
      if (std::holds_alternative<RecordBatchWithCache>(*it)) {
        auto record_batch_ptr = std::get_if<RecordBatchWithCache>(&(*it));
        for (int64_t col_idx = 0; col_idx < static_cast<int64_t>(rel_.NumColumns()); ++col_idx) {
          if (record_batch_ptr->cache_validity[col_idx]) {
            PL_RETURN_IF_ERROR(
                builder.AppendColumn(col_idx, record_batch_ptr->arrow_cache[col_idx]));
          } else {
            PL_RETURN_IF_ERROR(builder.AppendColumn(
                col_idx, record_batch_ptr->record_batch->at(col_idx)->ConvertToArrow(mem_pool)));
          }
        }
      } else {
        auto row_batch = std::get<schema::RowBatch>(*it);
        for (auto [col_idx, col] : Enumerate(row_batch.columns())) {
          PL_RETURN_IF_ERROR(builder.AppendColumn(col_idx, col));
        }
      }
      auto row_ids = hot_row_ids_.front();
      if (first_row_id == -1) {
        first_row_id = row_ids.first;
      }
      last_row_id = row_ids.second;

      it = hot_batches_.erase(it);
      hot_row_ids_.pop_front();

      if (time_col_idx_ != -1) {
        auto times = hot_time_.front();
        if (first_time == -1) {
          first_time = times.first;
        }
        last_time = times.second;
        hot_time_.pop_front();
      }
    }
  }
  PL_RETURN_IF_ERROR(builder.Finish());
  {
    absl::MutexLock cold_lock(&cold_lock_);
    PL_RETURN_IF_ERROR(AdvanceRingBufferUnlocked());
    for (const auto& [col_idx, col] : Enumerate(builder.output_columns())) {
      cold_column_buffers_[col_idx][ring_back_idx_] = col;
    }
    cold_row_ids_.emplace_back(first_row_id, last_row_id);
    if (time_col_idx_ != -1) {
      cold_time_.emplace_back(first_time, last_time);
    }
  }
  {
    absl::base_internal::SpinLockHolder stat_lock(&stats_lock_);
    hot_bytes_ -= builder.Size();
    cold_bytes_ += builder.Size();
    compacted_batches_++;
  }
  generation_++;
  return Status::OK();
}

Status Table::CompactHotToCold(arrow::MemoryPool* mem_pool) {
  for (size_t i = 0; i < kMaxBatchesPerCompactionCall; ++i) {
    {
      absl::base_internal::SpinLockHolder stats_lock(&stats_lock_);
      if (hot_bytes_ < min_cold_batch_size_) {
        return Status::OK();
      }
    }
    PL_RETURN_IF_ERROR(CompactSingleBatch(mem_pool));
  }

  return Status::OK();
}

StatusOr<bool> Table::ExpireCold() {
  int64_t rb_bytes = 0;
  {
    absl::MutexLock gen_lock(&generation_lock_);
    absl::MutexLock cold_lock(&cold_lock_);
    if (RingSizeUnlocked() == 0) {
      return false;
    }
    cold_row_ids_.pop_front();
    if (time_col_idx_ != -1) cold_time_.pop_front();

    for (size_t col_idx = 0; col_idx < rel_.NumColumns(); col_idx++) {
#define TYPE_CASE(_dt_) \
  rb_bytes += types::GetArrowArrayBytes<_dt_>(cold_column_buffers_[col_idx][ring_front_idx_].get());
      PL_SWITCH_FOREACH_DATATYPE(rel_.GetColumnType(col_idx), TYPE_CASE);
#undef TYPE_CASE
      cold_column_buffers_[col_idx][ring_front_idx_].reset();
    }
    if (ring_front_idx_ == ring_back_idx_) {
      // The batch we are expiring is the last batch in the ring buffer, so we reset the indices.
      ring_front_idx_ = 0;
      ring_back_idx_ = -1;
    } else {
      ring_front_idx_ = (ring_front_idx_ + 1) % ring_capacity_;
    }
    generation_++;
  }
  absl::base_internal::SpinLockHolder lock(&stats_lock_);
  cold_bytes_ -= rb_bytes;
  return true;
}

Status Table::ExpireHot() {
  RecordOrRowBatch record_or_row_batch;
  {
    absl::MutexLock gen_lock(&generation_lock_);
    absl::MutexLock hot_lock(&hot_lock_);
    if (hot_batches_.size() == 0) {
      return error::InvalidArgument("Failed to expire row batch, no row batches in table");
    }
    if (time_col_idx_ != -1) hot_time_.pop_front();
    hot_row_ids_.pop_front();
    record_or_row_batch = std::move(hot_batches_.front());
    hot_batches_.pop_front();
    // Expire the first hot batch invalidates all hot indices, so we have to increase the
    // generation.
    generation_++;
  }
  int64_t rb_bytes = 0;
  if (std::holds_alternative<RecordBatchWithCache>(record_or_row_batch)) {
    auto record_batch = std::move(std::get<RecordBatchWithCache>(record_or_row_batch));
    for (const auto& col : *record_batch.record_batch) {
      rb_bytes += col->Bytes();
    }
  } else {
    auto row_batch = std::get<schema::RowBatch>(record_or_row_batch);
    for (const auto& [col_idx, col] : Enumerate(row_batch.columns())) {
#define TYPE_CASE(_dt_) rb_bytes += types::GetArrowArrayBytes<_dt_>(col.get());
      PL_SWITCH_FOREACH_DATATYPE(rel_.GetColumnType(col_idx), TYPE_CASE);
#undef TYPE_CASE
    }
  }
  {
    absl::base_internal::SpinLockHolder lock(&stats_lock_);
    hot_bytes_ -= rb_bytes;
  }
  return Status::OK();
}

Status Table::ExpireBatch() {
  PL_ASSIGN_OR_RETURN(auto expired_cold, ExpireCold());
  if (expired_cold) {
    return Status::OK();
  }
  // If we get to this point then there were no cold batches to expire, so we try to expire a hot
  // batch.
  return ExpireHot();
}

Status Table::AddBatchSliceToRowBatch(const BatchSlice& slice, const std::vector<int64_t>& cols,
                                      schema::RowBatch* output_rb,
                                      arrow::MemoryPool* mem_pool) const {
  absl::MutexLock gen_lock(&generation_lock_);
  PL_RETURN_IF_ERROR(UpdateSliceUnlocked(slice));
  // After this point, as long as gen_lock is held, the unsafe properties of slice are valid.
  if (!slice.unsafe_is_hot) {
    absl::MutexLock cold_lock(&cold_lock_);
    for (auto col_idx : cols) {
      auto arr = cold_column_buffers_[col_idx][slice.unsafe_batch_index]->Slice(
          slice.unsafe_row_start, slice.unsafe_row_end + 1 - slice.unsafe_row_start);
      PL_RETURN_IF_ERROR(output_rb->AddColumn(arr));
    }
    return Status::OK();
  }

  absl::MutexLock hot_lock(&hot_lock_);
  if (std::holds_alternative<RecordBatchWithCache>(hot_batches_[slice.unsafe_batch_index])) {
    auto record_batch_ptr =
        std::get_if<RecordBatchWithCache>(&hot_batches_[slice.unsafe_batch_index]);
    for (auto col_idx : cols) {
      if (record_batch_ptr->cache_validity[col_idx]) {
        auto arr = record_batch_ptr->arrow_cache[col_idx]->Slice(
            slice.unsafe_row_start, slice.unsafe_row_end + 1 - slice.unsafe_row_start);
        PL_RETURN_IF_ERROR(output_rb->AddColumn(arr));
        continue;
      }
      // Arrow array wasn't in cache, Convert to arrow and then add to cache.
      auto arr = record_batch_ptr->record_batch->at(col_idx)->ConvertToArrow(mem_pool);
      record_batch_ptr->arrow_cache[col_idx] = arr;
      record_batch_ptr->cache_validity[col_idx] = true;
      PL_RETURN_IF_ERROR(output_rb->AddColumn(
          arr->Slice(slice.unsafe_row_start, slice.unsafe_row_end + 1 - slice.unsafe_row_start)));
    }
  } else {
    const auto& row_batch = std::get<schema::RowBatch>(hot_batches_[slice.unsafe_batch_index]);
    for (auto col_idx : cols) {
      auto arr = row_batch.ColumnAt(col_idx)->Slice(
          slice.unsafe_row_start, slice.unsafe_row_end + 1 - slice.unsafe_row_start);
      PL_RETURN_IF_ERROR(output_rb->AddColumn(arr));
    }
  }
  return Status::OK();
}

int64_t Table::NumBatches() const {
  absl::MutexLock gen_lock(&generation_lock_);
  absl::MutexLock cold_lock(&cold_lock_);
  absl::MutexLock hot_lock(&hot_lock_);
  return RingSizeUnlocked() + hot_batches_.size();
}

BatchSlice Table::FirstBatch() const {
  absl::MutexLock gen_lock(&generation_lock_);
  {
    absl::MutexLock cold_lock(&cold_lock_);
    if (ring_back_idx_ != -1) {
      auto row_ids = cold_row_ids_.front();
      return BatchSlice::Cold(ring_front_idx_, 0, ColdBatchLengthUnlocked(ring_front_idx_) - 1,
                              generation_, row_ids);
    }
  }
  // No cold batches, return first hot batch or invalid if there are no hot batches.
  absl::MutexLock hot_lock(&hot_lock_);
  if (hot_batches_.size() == 0) {
    return BatchSlice::Invalid();
  }
  auto row_ids = hot_row_ids_.front();
  return BatchSlice::Hot(0, 0, HotBatchLengthUnlocked(0) - 1, generation_, row_ids);
}

int64_t Table::End() const {
  absl::MutexLock hot_lock(&hot_lock_);
  return next_row_id_;
}

BatchSlice Table::NextBatch(const BatchSlice& slice, int64_t stop_row_id) const {
  auto next_slice = NextBatchWithoutStop(slice);
  return SliceIfPastStop(next_slice, stop_row_id);
}

BatchSlice Table::NextBatchWithoutStop(const BatchSlice& slice) const {
  absl::MutexLock gen_lock(&generation_lock_);
  auto status = UpdateSliceUnlocked(slice);
  if (!status.ok()) {
    return BatchSlice::Invalid();
  }
  if (!slice.unsafe_is_hot) {
    absl::MutexLock cold_lock(&cold_lock_);
    auto batch_length = ColdBatchLengthUnlocked(slice.unsafe_batch_index);
    // We first check if the previous slice had already output all the rows in its batch. If it
    // didn't then we need to output a batch with the remaining rows.
    if (slice.unsafe_row_end < batch_length - 1) {
      auto new_batch_size = batch_length - slice.unsafe_row_end;
      return BatchSlice::Cold(slice.unsafe_batch_index, slice.unsafe_row_end + 1, batch_length - 1,
                              generation_, slice.uniq_row_end_idx + 1,
                              slice.uniq_row_end_idx + new_batch_size - 1);
    }

    auto next_ring_index = RingNextAddrUnlocked(slice.unsafe_batch_index);
    if (next_ring_index == -1) {
      absl::MutexLock hot_lock(&hot_lock_);
      // This is the last cold batch so return the first hot batch. If there are no hot batches
      // return an invalid batch.
      if (hot_batches_.size() == 0) {
        return BatchSlice::Invalid();
      }
      return BatchSlice::Hot(0, 0, HotBatchLengthUnlocked(0) - 1, generation_,
                             hot_row_ids_.front());
    }

    // At this point we can just take the next cold batch.
    return BatchSlice::Cold(next_ring_index, 0, ColdBatchLengthUnlocked(next_ring_index) - 1,
                            generation_, cold_row_ids_[RingVectorIndexUnlocked(next_ring_index)]);
  }

  absl::MutexLock hot_lock(&hot_lock_);
  auto batch_length = HotBatchLengthUnlocked(slice.unsafe_batch_index);
  if (slice.unsafe_row_end < batch_length - 1) {
    auto new_batch_size = batch_length - slice.unsafe_row_end;
    return BatchSlice::Hot(slice.unsafe_batch_index, slice.unsafe_row_end + 1, batch_length - 1,
                           generation_, slice.uniq_row_end_idx + 1,
                           slice.uniq_row_end_idx + new_batch_size - 1);
  }

  auto next_index = slice.unsafe_batch_index + 1;
  if (next_index >= static_cast<int64_t>(hot_batches_.size())) {
    return BatchSlice::Invalid();
  }
  auto next_length = HotBatchLengthUnlocked(next_index);
  return BatchSlice::Hot(next_index, 0, next_length - 1, generation_, hot_row_ids_[next_index]);
}

int64_t Table::FindStopTime(int64_t time, arrow::MemoryPool* mem_pool) const {
  absl::MutexLock gen_lock(&generation_lock_);
  {
    absl::MutexLock hot_lock(&hot_lock_);
    auto it =
        std::upper_bound(hot_time_.begin(), hot_time_.end(), time, IntervalComparatorUpperBound);
    if (it != hot_time_.begin()) {
      it--;
      auto index = std::distance(hot_time_.begin(), it);
      ArrowArrayPtr time_col;
      if (std::holds_alternative<RecordBatchWithCache>(hot_batches_[index])) {
        auto record_batch_ptr = std::get_if<RecordBatchWithCache>(&hot_batches_[index]);
        time_col = GetHotColumnUnlocked(record_batch_ptr, time_col_idx_, mem_pool);
      } else {
        auto row_batch = std::get<schema::RowBatch>(hot_batches_[index]);
        time_col = row_batch.columns().at(time_col_idx_);
      }
      auto row_offset =
          types::SearchArrowArrayLessThanOrEqual<types::DataType::TIME64NS>(time_col.get(), time);
      return hot_row_ids_[index].first + row_offset;
    }
  }
  absl::MutexLock cold_lock(&cold_lock_);
  auto it =
      std::upper_bound(cold_time_.begin(), cold_time_.end(), time, IntervalComparatorUpperBound);
  if (it == cold_time_.begin()) {
    return -1;
  }
  it--;
  auto index = it - cold_time_.begin();
  auto ring_index = RingIndexUnlocked(index);
  auto time_col = cold_column_buffers_[time_col_idx_][ring_index];
  auto row_offset =
      types::SearchArrowArrayLessThanOrEqual<types::DataType::TIME64NS>(time_col.get(), time);
  return cold_row_ids_[index].first + row_offset;
}

int64_t Table::ColdBatchLengthUnlocked(int64_t index) const {
  return cold_column_buffers_[0].at(index)->length();
}
int64_t Table::HotBatchLengthUnlocked(int64_t index) const {
  if (std::holds_alternative<RecordBatchWithCache>(hot_batches_[index])) {
    auto record_batch_ptr = std::get_if<RecordBatchWithCache>(&hot_batches_[index]);
    return record_batch_ptr->record_batch->at(0)->Size();
  } else {
    auto row_batch = std::get<schema::RowBatch>(hot_batches_[index]);
    return row_batch.num_rows();
  }
}

Table::ArrowArrayPtr Table::GetHotColumnUnlocked(const RecordBatchWithCache* record_batch_ptr,
                                                 int64_t col_idx,
                                                 arrow::MemoryPool* mem_pool) const {
  if (record_batch_ptr->cache_validity[col_idx]) {
    return record_batch_ptr->arrow_cache[col_idx];
  }
  auto arrow_array_sptr = record_batch_ptr->record_batch->at(col_idx)->ConvertToArrow(mem_pool);
  record_batch_ptr->arrow_cache[col_idx] = arrow_array_sptr;
  record_batch_ptr->cache_validity[col_idx] = true;
  return arrow_array_sptr;
}

BatchSlice Table::SliceIfPastStop(const BatchSlice& slice, int64_t stop_row_id) const {
  if (!slice.IsValid()) {
    return slice;
  }

  if (slice.uniq_row_start_idx >= stop_row_id) {
    return BatchSlice::Invalid();
  }
  if (slice.uniq_row_end_idx >= stop_row_id) {
    auto new_slice = slice;
    new_slice.uniq_row_end_idx = stop_row_id - 1;
    // Force update of unsafe properties.
    new_slice.generation = -1;
    return new_slice;
  }
  return slice;
}

int64_t Table::RingVectorIndexUnlocked(int64_t ring_index) const {
  // This function assumes the ring_index is valid. If it's not valid the resulting vector index
  // will also not be valid.
  if (ring_index >= ring_front_idx_) {
    return ring_index - ring_front_idx_;
  }
  return ring_index + (ring_capacity_ - ring_front_idx_);
}

int64_t Table::RingIndexUnlocked(int64_t vector_index) const {
  // This function assumes the vector_index is valid. If it's not valid the resulting ring index
  // will also not be valid.
  return (ring_front_idx_ + vector_index) % ring_capacity_;
}

int64_t Table::RingSizeUnlocked() const {
  if (ring_back_idx_ == -1) {
    return 0;
  }
  if (ring_front_idx_ <= ring_back_idx_) {
    return 1 + (ring_back_idx_ - ring_front_idx_);
  }
  return (ring_back_idx_ + 1) + (ring_capacity_ - ring_front_idx_);
}

int64_t Table::RingNextAddrUnlocked(int64_t ring_index) const {
  // This function assumes the passed in ring_index is valid, and then checks that the next index is
  // valid.
  if (ring_back_idx_ == -1 || ring_index == ring_back_idx_) {
    return -1;
  }
  return (ring_index + 1) % ring_capacity_;
}

Status Table::AdvanceRingBufferUnlocked() {
  auto next_ring_back_idx = (ring_back_idx_ + 1) % ring_capacity_;
  if (ring_back_idx_ != -1 && next_ring_back_idx == ring_front_idx_) {
    return error::Internal(
        "Failed to add new cold batch, ring buffer alreadRingNextAddrUnlocked full");
  }
  ring_back_idx_ = next_ring_back_idx;
  return Status::OK();
}

Status Table::UpdateSliceUnlocked(const BatchSlice& slice) const {
  if (slice.generation == generation_) {
    return Status::OK();
  }
  {
    absl::MutexLock cold_lock(&cold_lock_);
    auto it = std::lower_bound(cold_row_ids_.begin(), cold_row_ids_.end(), slice.uniq_row_start_idx,
                               IntervalComparatorLowerBound);

    if (it != cold_row_ids_.end()) {
      if (slice.uniq_row_end_idx < it->first) {
        // All data in this slice has been expired from the table.
        return error::InvalidArgument(
            "Requested RowBatch Slice has already been expired from the table");
      }
      auto vector_index = std::distance(cold_row_ids_.begin(), it);
      auto ring_index = RingIndexUnlocked(vector_index);
      slice.unsafe_is_hot = false;
      slice.unsafe_batch_index = ring_index;
      slice.unsafe_row_start = slice.uniq_row_start_idx - it->first;
      slice.unsafe_row_end = slice.uniq_row_end_idx - it->first;
      slice.generation = generation_;
      return Status::OK();
    }
  }
  absl::MutexLock hot_lock(&hot_lock_);
  auto it = std::lower_bound(hot_row_ids_.begin(), hot_row_ids_.end(), slice.uniq_row_start_idx,
                             IntervalComparatorLowerBound);
  if (it == hot_row_ids_.end()) {
    return error::InvalidArgument("Request RowBatch Slice is not in bounds of the table");
  }
  if (slice.uniq_row_end_idx < it->first) {
    // All data in this slice has been expired from the table.
    return error::InvalidArgument(
        "Requested RowBatch Slice has already been expired from the table");
  }
  slice.unsafe_is_hot = true;
  slice.unsafe_batch_index = std::distance(hot_row_ids_.begin(), it);
  slice.unsafe_row_start = slice.uniq_row_start_idx - it->first;
  slice.unsafe_row_end = slice.uniq_row_end_idx - it->first;
  slice.generation = generation_;
  return Status::OK();
}

}  // namespace table_store
}  // namespace px
