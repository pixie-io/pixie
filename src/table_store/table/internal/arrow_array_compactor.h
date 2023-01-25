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

#include <memory>
#include <utility>
#include <vector>

#include "src/table_store/schema/relation.h"
#include "src/table_store/table/internal/record_or_row_batch.h"
#include "src/table_store/table/internal/types.h"

namespace px {
namespace table_store {
namespace internal {

/**
 * ArrowArrayCompactor compacts smaller row batches into a single row batch in the form of an
 * arrow::Array for each column in the row batch. ArrowArrayCompactor can accept either
 * `RecordBatchWithCache` objects or `schema::RowBatch` objects, through the variant type
 * `RecordOrRowBatch`. It also supports appending only a slice of a given row batch. Typical usage
 * should be as follows:
 *
 *  PX_RETURN_IF_ERROR(compactor.Reserve(num_rows, variable_col_sizes_bytes));
 *  for (auto record_or_row_batch : record_or_row_batches_to_compact) {
 *    compactor.UnsafeAppendBatchSlice(record_or_row_batch, 0, NumRows(record_or_row_batch));
 *  }
 *  auto output_arrow_arrays = compactor.Finish();
 */
class ArrowArrayCompactor {
 public:
  ArrowArrayCompactor(const schema::Relation& rel, arrow::MemoryPool* mem_pool);
  /**
   * Reserve space for the given number of rows, and in the case of binary column types (eg. string
   * columns) reserve space for columns data given by col_size_bytes.
   * To prevent extraneous copies/allocations, `Reserve` should be called once with the total size
   * of the compacted batch.
   * @param num_rows Number of rows needed for the compacted batch.
   * @param variable_col_size_bytes Vector of sizes in bytes for each column, ignoring fixed costs.
   * For all columns that aren't strings, this should be 0. For string columns, we need to reserve
   * space for the strings themselves. For example, for a string column with 2 rows: "a" and "abc",
   * the value of variable_col_size_bytes[col_idx] should be 1+3=4.
   * @return Status
   */
  Status Reserve(size_t num_rows, const std::vector<size_t>& variable_col_size_bytes);
  /**
   * Append a slice of the given RecordBatchWithCache or schema::RowBatch, to the compacted batch,
   * starting at `start_row` and including all rows up to but not including `end_row`.
   * It is required to call `Reserve` first with the total number of rows and col sizes, for all
   * slices that will be appended through UnsafeAppendBatchSlice.
   * @param batch A std::variant containing either a RecordBatchWithCache or a schema::RowBatch.
   * @param start_row Row index in `batch` to start appending from
   * @param end_row Row index in `batch` to stop appending at (non-inclusive of `end_row`)
   */
  void UnsafeAppendBatchSlice(const RecordOrRowBatch& batch, size_t start_row, size_t end_row);
  /**
   * Return the compacted arrow::Array's for each column.
   * @return compacted arrow::Array's per column in the batch.
   */
  StatusOr<std::vector<ArrowArrayPtr>> Finish();

 private:
  const schema::Relation& rel_;
  std::vector<std::unique_ptr<types::TypeErasedArrowBuilder>> builders_;
};

}  // namespace internal
}  // namespace table_store
}  // namespace px
