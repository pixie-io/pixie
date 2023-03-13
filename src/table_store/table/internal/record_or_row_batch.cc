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

#include <vector>

#include "src/common/base/utils.h"
#include "src/table_store/table/internal/record_or_row_batch.h"

namespace px {
namespace table_store {
namespace internal {

size_t RecordOrRowBatch::Length() const {
  return std::visit(overloaded{
                        [this](const RecordBatchWithCache& record_batch_w_cache) {
                          const auto& record_batch = *record_batch_w_cache.record_batch;
                          DCHECK(!record_batch.empty());
                          return record_batch[0]->Size() - row_offset_;
                        },
                        [this](const schema::RowBatch& row_batch) {
                          return static_cast<size_t>(row_batch.num_rows()) - row_offset_;
                        },
                    },
                    batch_);
}

int64_t RecordOrRowBatch::FindTimeFirstGreaterThanOrEqual(int64_t time_col_idx, Time time) const {
  return std::visit(
      overloaded{
          [this, time, time_col_idx](const RecordBatchWithCache& record_batch_w_cache) -> int64_t {
            auto col = (*record_batch_w_cache.record_batch)[time_col_idx];
            auto iterable = types::ColumnWrapperIterator<types::DataType::TIME64NS>(col.get());
            auto it = std::lower_bound(iterable.begin() + row_offset_, iterable.end(), time);
            if (it == iterable.end()) {
              return -1;
            }
            return std::distance(iterable.begin(), it) - row_offset_;
          },
          [this, time, time_col_idx](const schema::RowBatch& row_batch) {
            auto length = row_batch.num_rows() - row_offset_;
            auto arr = row_batch.ColumnAt(time_col_idx)->Slice(row_offset_, length);
            return types::SearchArrowArrayGreaterThanOrEqual<types::DataType::TIME64NS>(arr.get(),
                                                                                        time);
          },
      },
      batch_);
}

int64_t RecordOrRowBatch::FindTimeFirstGreaterThan(int64_t time_col_idx, Time time) const {
  return std::visit(
      overloaded{
          [this, time, time_col_idx](const RecordBatchWithCache& record_batch_w_cache) -> int64_t {
            auto col = (*record_batch_w_cache.record_batch)[time_col_idx];
            auto iterable = types::ColumnWrapperIterator<types::DataType::TIME64NS>(col.get());
            auto it = std::upper_bound(iterable.begin() + row_offset_, iterable.end(), time);
            if (it == iterable.end()) {
              return -1;
            }
            return std::distance(iterable.begin(), it) - row_offset_;
          },
          [this, time, time_col_idx](const schema::RowBatch& row_batch) -> int64_t {
            auto length = row_batch.num_rows() - row_offset_;
            auto arr = row_batch.ColumnAt(time_col_idx)->Slice(row_offset_, length);
            if (time >=
                types::GetValueFromArrowArray<types::DataType::TIME64NS>(arr.get(), length - 1)) {
              return -1;
            }
            return types::SearchArrowArrayLessThanOrEqual<types::DataType::TIME64NS>(arr.get(),
                                                                                     time) +
                   1;
          },
      },
      batch_);
}

Time RecordOrRowBatch::GetTimeValue(int64_t time_col_idx, int64_t row_idx) const {
  row_idx += row_offset_;
  return std::visit(overloaded{
                        [time_col_idx, row_idx](const RecordBatchWithCache& record_batch_w_cache) {
                          auto col = (*record_batch_w_cache.record_batch)[time_col_idx];
                          return col->template Get<types::Time64NSValue>(row_idx).val;
                        },
                        [time_col_idx, row_idx](const schema::RowBatch& row_batch) {
                          return types::GetValueFromArrowArray<types::DataType::TIME64NS>(
                              row_batch.ColumnAt(time_col_idx).get(), row_idx);
                        },
                    },
                    batch_);
}

void RecordOrRowBatch::RemovePrefix(size_t num_rows) { row_offset_ += num_rows; }

Status RecordOrRowBatch::AddBatchSliceToRowBatch(size_t row_start, size_t batch_size,
                                                 const std::vector<int64_t>& cols,
                                                 schema::RowBatch* output_rb) const {
  row_start += row_offset_;
  return std::visit(
      overloaded{
          [row_start, batch_size, cols,
           output_rb](const RecordBatchWithCache& record_batch_w_cache) {
            for (auto col_idx : cols) {
              if (!record_batch_w_cache.cache_validity[col_idx]) {
                // Arrow array wasn't in cache, convert it to arrow and then add
                // to cache.
                auto arr = (*record_batch_w_cache.record_batch)[col_idx]->ConvertToArrow(
                    arrow::default_memory_pool());
                record_batch_w_cache.arrow_cache[col_idx] = arr;
                record_batch_w_cache.cache_validity[col_idx] = true;
              }
              auto arr = record_batch_w_cache.arrow_cache[col_idx]->Slice(row_start, batch_size);
              PX_RETURN_IF_ERROR(output_rb->AddColumn(arr));
            }
            return Status::OK();
          },
          [row_start, batch_size, cols, output_rb](const schema::RowBatch& row_batch) {
            for (auto col_idx : cols) {
              auto arr = row_batch.ColumnAt(col_idx)->Slice(row_start, batch_size);
              PX_RETURN_IF_ERROR(output_rb->AddColumn(arr));
            }
            return Status::OK();
          },
      },
      batch_);
}

void RecordOrRowBatch::UnsafeAppendColumnToBuilder(types::TypeErasedArrowBuilder* builder,
                                                   types::DataType data_type, int64_t col_idx,
                                                   size_t start_row, size_t end_row) const {
  start_row += row_offset_;
  end_row += row_offset_;
  std::visit(
      overloaded{
          [builder, data_type, col_idx, start_row,
           end_row](const RecordBatchWithCache& record_batch_w_cache) {
            const auto& record_batch = *record_batch_w_cache.record_batch;
#define TYPE_CASE(_dt_)                                                            \
  auto iterable = types::ColumnWrapperIterator<_dt_>(record_batch[col_idx].get()); \
  auto typed_builder = types::GetTypedArrowBuilder<_dt_>(builder);                 \
  typed_builder->UnsafeAppendValues(iterable.begin() + start_row, iterable.begin() + end_row);
            PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE
          },
          [builder, data_type, col_idx, start_row, end_row](const schema::RowBatch& row_batch) {
#define TYPE_CASE(_dt_)                                                               \
  auto iterable = types::ArrowArrayIterator<_dt_>(row_batch.ColumnAt(col_idx).get()); \
  auto typed_builder = types::GetTypedArrowBuilder<_dt_>(builder);                    \
  typed_builder->UnsafeAppendValues(iterable.begin() + start_row, iterable.begin() + end_row);
            PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE
          },
      },
      batch_);
}

std::vector<uint64_t> RecordOrRowBatch::GetVariableSizedColumnRowBytes(size_t col_idx) const {
  std::vector<uint64_t> rows_bytes;
  // Currently, types::DataType::STRING is the only supported data type that has variable sized
  // rows. So this method, only operators on string columns at the moment.

  std::visit(overloaded{
                 [this, &rows_bytes, col_idx](const RecordBatchWithCache& record_batch_w_cache) {
                   const auto& record_batch = *record_batch_w_cache.record_batch;
                   auto* col_wrapper = record_batch[col_idx].get();
                   for (size_t i = row_offset_; i < col_wrapper->Size(); ++i) {
                     rows_bytes.push_back(col_wrapper->GetView(i).size());
                   }
                 },
                 [this, &rows_bytes, col_idx](const schema::RowBatch& row_batch) {
                   auto* arrow_arr = row_batch.ColumnAt(col_idx).get();
                   for (int64_t i = row_offset_; i < arrow_arr->length(); ++i) {
                     rows_bytes.push_back(types::GetStringViewFromArrowArray(arrow_arr, i).size());
                   }
                 },
             },
             batch_);

  return rows_bytes;
}

}  // namespace internal
}  // namespace table_store
}  // namespace px
