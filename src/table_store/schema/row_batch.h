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

#include <arrow/array.h>
#include <arrow/type.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/schemapb/schema.pb.h"

namespace px {
namespace table_store {
namespace schema {

/**
 * A RowBatch is a table-like structure which consists of equal-length arrays
 * that match the schema described by the RowDescriptor.
 */
class RowBatch {
 public:
  /**
   * Creates a row batch.
   *
   * @ param desc the descriptor which describes the schema of the row batch
   * @ param num_rows the number of rows that the row batch should contain.
   */
  RowBatch(RowDescriptor desc, int64_t num_rows) : desc_(std::move(desc)), num_rows_(num_rows) {
    columns_.reserve(desc_.size());
  }

  Status ToProto(table_store::schemapb::RowBatchData* row_batch_proto) const;
  static StatusOr<std::unique_ptr<RowBatch>> FromProto(
      const table_store::schemapb::RowBatchData& row_batch_proto);

  static StatusOr<std::unique_ptr<RowBatch>> FromColumnBuilders(
      const RowDescriptor& desc, bool eow, bool eos,
      std::vector<std::unique_ptr<arrow::ArrayBuilder>>* builders);

  /**
   * Creates a row batch with zero rows in it matching the row descriptor.
   */
  static StatusOr<std::unique_ptr<RowBatch>> WithZeroRows(const RowDescriptor& desc, bool eow,
                                                          bool eos);

  /**
   * @brief Returns a slice of the specified `length` starting at the `offset` from the RowBatch.
   *
   * RowBatch Slice has the same columns as this rowbatch, just of length `length` and starting at
   * `offset`. Does not set eow and eos.
   *
   *
   * @param offset The starting position of the slice.
   * @param offset The length of the slice to grab.
   * @return StatusOr<std::unique_ptr<RowBatch>>
   */
  StatusOr<std::unique_ptr<RowBatch>> Slice(int64_t offset, int64_t length) const;

  /**
   * Adds the given column to the row batch, given that it correctly fits the schema.
   * param col ptr to the arrow array that should be added to the row batch.
   */
  Status AddColumn(const std::shared_ptr<arrow::Array>& col);

  /**
   * @ param i the index of the column to be accessed.
   * @ returns the Arrow array for the column at the given index.
   */
  std::shared_ptr<arrow::Array> ColumnAt(int64_t i) const;

  /**
   * @ param i the index of the column to check.
   * @ returns whether the rowbatch contains a column at the given index.
   */
  bool HasColumn(int64_t i) const;

  /**
   * @ return the number of rows that each row batch should contain.
   */
  int64_t num_rows() const { return num_rows_; }

  /**
   * @ return the number of columns which the row batch should contain.
   */
  int64_t num_columns() const { return desc_.size(); }

  // eow (end of window) denotes whether the row batch is the last batch for its window.
  bool eow() const { return eow_; }
  void set_eow(bool val) { eow_ = val; }

  bool eos() const { return eos_; }
  void set_eos(bool val) { eos_ = val; }
  /**
   * @ return the row descriptor which describes the schema of the row batch.
   */
  const RowDescriptor& desc() const { return desc_; }

  std::string DebugString() const;
  std::vector<std::shared_ptr<arrow::Array>> columns() const { return columns_; }

  int64_t NumBytes() const;

 private:
  RowDescriptor desc_;
  int64_t num_rows_;
  bool eow_ = false;
  bool eos_ = false;
  std::vector<std::shared_ptr<arrow::Array>> columns_;
};

// Append a scalar value to an arrow::Array.
template <types::DataType T>
Status CopyValue(arrow::ArrayBuilder* output_col_builder,
                 const typename px::types::DataTypeTraits<T>::native_type& value) {
  auto* typed_col_builder =
      static_cast<typename types::DataTypeTraits<T>::arrow_builder_type*>(output_col_builder);

  if constexpr (T == types::DataType::STRING) {
    int64_t size = value.size() + typed_col_builder->value_data_length();
    if (size >= typed_col_builder->value_data_capacity()) {
      PX_RETURN_IF_ERROR(typed_col_builder->ReserveData(std::lrint(1.5 * size)));
    }
  }

  typed_col_builder->UnsafeAppend(value);
  return Status::OK();
}

template <types::DataType T>
Status CopyValueRepeated(arrow::ArrayBuilder* output_col_builder,
                         const typename px::types::DataTypeTraits<T>::native_type& value,
                         size_t num_times) {
  auto* typed_col_builder =
      static_cast<typename types::DataTypeTraits<T>::arrow_builder_type*>(output_col_builder);

  if constexpr (T == types::DataType::STRING) {
    int64_t new_size = num_times * value.size() + typed_col_builder->value_data_length();
    if (new_size >= typed_col_builder->value_data_capacity()) {
      PX_RETURN_IF_ERROR(typed_col_builder->ReserveData(std::lrint(1.5 * new_size)));
    }
  }
  for (size_t i = 0; i < num_times; ++i) {
    typed_col_builder->UnsafeAppend(value);
  }
  return Status::OK();
}

}  // namespace schema
}  // namespace table_store
}  // namespace px
