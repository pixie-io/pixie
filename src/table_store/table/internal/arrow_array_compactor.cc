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

#include "src/table_store/table/internal/arrow_array_compactor.h"
#include "src/table_store/table/internal/record_or_row_batch.h"

namespace px {
namespace table_store {
namespace internal {

ArrowArrayCompactor::ArrowArrayCompactor(const schema::Relation& rel, arrow::MemoryPool* mem_pool)
    : rel_(rel) {
  for (const auto& type : rel_.col_types()) {
    builders_.push_back(types::MakeTypeErasedArrowBuilder(type, mem_pool));
  }
}

Status ArrowArrayCompactor::Reserve(size_t num_rows,
                                    const std::vector<size_t>& variable_col_size_bytes) {
  for (const auto& [col_idx, builder] : Enumerate(builders_)) {
    PX_RETURN_IF_ERROR(builder->Reserve(num_rows));
    if (rel_.col_types()[col_idx] == types::DataType::STRING) {
      PX_RETURN_IF_ERROR(builder->ReserveData(variable_col_size_bytes[col_idx]));
    }
  }
  return Status::OK();
}

void ArrowArrayCompactor::UnsafeAppendBatchSlice(const RecordOrRowBatch& batch, size_t start_row,
                                                 size_t end_row) {
  for (const auto& [col_idx, builder] : Enumerate(builders_)) {
    batch.UnsafeAppendColumnToBuilder(builder.get(), rel_.col_types()[col_idx], col_idx, start_row,
                                      end_row);
  }
}

StatusOr<std::vector<ArrowArrayPtr>> ArrowArrayCompactor::Finish() {
  std::vector<ArrowArrayPtr> out_columns;
  for (const auto& [col_idx, builder] : Enumerate(builders_)) {
    out_columns.emplace_back();
    PX_RETURN_IF_ERROR(builder->Finish(&out_columns.back()));
  }
  return out_columns;
}

}  // namespace internal
}  // namespace table_store
}  // namespace px
