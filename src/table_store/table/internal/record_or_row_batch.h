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

#include <utility>
#include <variant>
#include <vector>

#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table/internal/types.h"

namespace px {
namespace table_store {
namespace internal {

/**
 * RecordOrRowBatch is a wrapper around an `std::variant` with `RecordBatchWithCache` and
 * `schema::RowBatch` as subtypes.
 *
 * The wrapper allows for uniform access to `RecordBatchWithCache` objects and `schema::RowBatch`
 * objects, so that all other table components can ignore the details of whether a given batch is a
 * `RecordBatchWithCache` or a `schema::RowBatch`. The wrapper also allows for removing rows from
 * the start of the batch without reallocating or copying the batch. To do so, it stores a
 * `row_offset_` internally, and each operation on a batch acts as if the batch actually starts at
 * `row_offset_`.
 */
class RecordOrRowBatch {
 public:
  explicit RecordOrRowBatch(RecordBatchWithCache&& record_batch)
      : batch_(std::move(record_batch)) {}
  explicit RecordOrRowBatch(const schema::RowBatch& row_batch) : batch_(row_batch) {}

  RecordOrRowBatch(RecordOrRowBatch&&) = default;

  /**
   * Length returns the number of rows in this record or row batch.
   * @return number of rows.
   */
  size_t Length() const;
  /**
   * FindTimeFirstGreaterThanOrEqual returns the first row index within this batch that has time
   * greater than or equal to the given time.
   * @param time_col_idx, column index to use for times.
   * @param time, the time to search for.
   * @return row index corresponding to the first row in this row batch that has time greather than
   * or equal to the given time, or -1 if no such row exists.
   */
  int64_t FindTimeFirstGreaterThanOrEqual(int64_t time_col_idx, Time time) const;
  /**
   * FindTimeFirstGreaterThan returns the first row index within this batch that has time
   * greater than the given time.
   * @param time_col_idx, column index to use for times.
   * @param time, the time to search for.
   * @return row index corresponding to the first row in this row batch that has time greather than
   * the given time, or -1 if no such row exists.
   */
  int64_t FindTimeFirstGreaterThan(int64_t time_col_idx, Time time) const;
  /**
   * GetTimeValue returns the value of the time column at the given row index.
   * @param time_col_idx, the index of the column to get the time value from.
   * @param row_idx, the index of the row to get.
   * @return the time value at the given row index.
   */
  Time GetTimeValue(int64_t time_col_idx, int64_t row_idx) const;
  /**
   * RemovePrefix removes the given number of rows from the start of this record or row batch. To
   * avoid reallocations, RecordOrRowBatch stores a `row_offset_` that is incremented by the number
   * of rows when this method is called. All other methods use `row_offset_` internally, such that
   * they return results as if `row_offset_` number of rows had been removed from the record or row
   * batch.
   * @param num_rows_to_remove number of rows to remove from the start.
   */
  void RemovePrefix(size_t num_rows_to_remove);
  /**
   * AddBatchSliceToRowBatch adds a slice of this record or row batch to the given output
   * schema::RowBatch.
   * @param row_start, row index within this batch to start the output slice at.
   * @param batch_size, size of the output slice.
   * @param cols, a vector of column indices to include in the output slice.
   * @param output_rb, a pointer to the row batch to add the columns to.
   * @return Status, errors if adding columns to the row batch fails.
   */
  Status AddBatchSliceToRowBatch(size_t row_start, size_t batch_size,
                                 const std::vector<int64_t>& cols,
                                 schema::RowBatch* output_rb) const;

  /**
   * UnsafeAppendColumnToBuilder appends a slice of a column of this record or row batch to the
   * given arrow array builder. This method expects that the given builder already has the space
   * reserved for appending the given column.
   * @param builder, pointer to a type erased arrow array builder.
   * @param col_data_type, the DataType of the column to be appended.
   * @param col_idx, index of the column to append.
   * @param start_row, index of the row to start the slice at.
   * @param end_row, index of the row to stop the slice at (the slice is non-inclusive of `end_row`)
   */
  void UnsafeAppendColumnToBuilder(types::TypeErasedArrowBuilder* builder,
                                   types::DataType col_data_type, int64_t col_idx, size_t start_row,
                                   size_t end_row) const;

  /**
   * GetVariableSizedColumnRowBytes returns the size of each row in a variable sized column (only
   * including the variable sized part, ignoring any fixed size for the row). Currently, the only
   * variable sized columns are string columns. So this method will return the size of each string
   * for a given string column. It also currently assumes the given column is a string column, and
   * is unsafe to call on other columns.
   * @param col_idx, index of the string column to get sizes for.
   * @return vector of sizes for each row in the column.
   */
  std::vector<uint64_t> GetVariableSizedColumnRowBytes(size_t col_idx) const;

 private:
  std::variant<RecordBatchWithCache, schema::RowBatch> batch_;
  int64_t row_offset_ = 0;
};

}  // namespace internal
}  // namespace table_store
}  // namespace px
