#include <arrow/array.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace exec {

Status Column::AddBatch(const std::shared_ptr<arrow::Array>& batch) {
  // Check type and check size.
  if (udf::CarnotToArrowType(data_type_) != batch->type_id()) {
    return error::InvalidArgument("Column is of type $0, but needs to be type $1.",
                                  batch->type_id(), data_type_);
  }

  batches_.emplace_back(batch);
  return Status::OK();
}

Status Table::AddColumn(std::shared_ptr<Column> col) {
  // Check number of columns.
  if (columns_.size() >= desc_.size()) {
    return error::InvalidArgument("Table has too many columns.");
  }

  // Check type of column.
  if (col->data_type() != desc_.type(columns_.size())) {
    return error::InvalidArgument("Column was is of type $0, but needs to be type $1.",
                                  col->data_type(), desc_.type(columns_.size()));
  }

  if (columns_.size() > 0) {
    // Check number of batches.
    if (columns_[0]->numBatches() != col->numBatches()) {
      return error::InvalidArgument("Column has $0 batches, but should have $1.", col->numBatches(),
                                    columns_[0]->numBatches());
    }
    // Check size of batches.
    for (int64_t batch_idx = 0; batch_idx < columns_[0]->numBatches(); batch_idx++) {
      if (columns_[0]->batch(batch_idx)->length() != col->batch(batch_idx)->length()) {
        return error::InvalidArgument("Column has batch of size $0, but should have size $1.",
                                      col->batch(batch_idx)->length(),
                                      columns_[0]->batch(batch_idx)->length());
      }
    }
  }

  columns_.emplace_back(col);
  name_to_column_map_.emplace(col->name(), col);
  return Status::OK();
}

StatusOr<std::unique_ptr<RowBatch>> Table::GetRowBatch(int64_t row_batch_idx,
                                                       std::vector<int64_t> cols,
                                                       arrow::MemoryPool* mem_pool) {
  DCHECK(columns_.size() > 0) << "RowBatch does not have any columns.";
  DCHECK(NumBatches() > row_batch_idx) << absl::StrFormat(
      "Table has %d batches, but requesting batch %d", NumBatches(), row_batch_idx);

  // Get column types for row descriptor.
  std::vector<udf::UDFDataType> rb_types;
  for (auto col_idx : cols) {
    rb_types.push_back(desc_.type(col_idx));
  }

  auto num_cold_batches = columns_.size() > 0 ? columns_[0]->numBatches() : 0;
  // If i > num_cold_batches, hot_idx is the index of the batch that we want from the hot columns.
  auto hot_idx = row_batch_idx - num_cold_batches;

  if (hot_idx >= 0) {
    DCHECK(hot_columns_.size() > static_cast<size_t>(hot_idx));
    // Move hot column batches 0 to hot_idx into cold storage.
    // TODO(michelle): (PL-388) We're currently converting hot data to row batches on a 1:1 basis.
    // This should be updated so that multiple hot column batches are merged into a single row
    // batch.
    auto batch_idx = 0;
    while (batch_idx <= hot_idx) {
      for (size_t col_idx = 0; col_idx < columns_.size(); col_idx++) {
        DCHECK(hot_columns_[batch_idx]->size() > col_idx);
        auto hot_batch_sptr = hot_columns_[batch_idx]->at(col_idx)->ConvertToArrow(mem_pool);
        PL_RETURN_IF_ERROR(columns_.at(col_idx)->AddBatch(hot_batch_sptr));
      }
      batch_idx++;
    }
    // Remove hot column batches 0 to hot_idx from hot columns.
    hot_columns_.erase(hot_columns_.begin(), hot_columns_.begin() + hot_idx + 1);
  }

  DCHECK(columns_.size() > 0 && columns_[0]->numBatches() > row_batch_idx);

  auto output_rb = std::make_unique<RowBatch>(RowDescriptor(rb_types),
                                              columns_[0]->batch(row_batch_idx)->length());
  for (auto col_idx : cols) {
    auto s = output_rb->AddColumn(columns_[col_idx]->batch(row_batch_idx));
    PL_RETURN_IF_ERROR(s);
  }

  return output_rb;
}

Status Table::WriteRowBatch(RowBatch rb) {
  if (rb.desc().size() != desc_.size()) {
    return error::InvalidArgument(
        "RowBatch's row descriptor length ($0) does not match table's row descriptor length ($1).",
        rb.desc().size(), desc_.size());
  }
  for (size_t i = 0; i < rb.desc().size(); i++) {
    if (rb.desc().type(i) != desc_.type(i)) {
      return error::InvalidArgument(
          "RowBatch's row descriptor does not match table's row descriptor.");
    }
  }

  for (int64_t i = 0; i < rb.num_columns(); i++) {
    auto s = columns_[i]->AddBatch(rb.ColumnAt(i));
    PL_RETURN_IF_ERROR(s);
  }
  return Status::OK();
}

int64_t Table::NumBatches() {
  auto num_batches = 0;
  if (columns_.size() > 0) {
    num_batches += columns_[0]->numBatches();
  }
  num_batches += hot_columns_.size();
  return num_batches;
}

plan::Relation Table::GetRelation() {
  std::vector<udf::UDFDataType> types;
  std::vector<std::string> names;
  types.reserve(columns_.size());
  names.reserve(columns_.size());

  for (const auto& col : columns_) {
    types.push_back(col->data_type());
    names.push_back(col->name());
  }

  return plan::Relation(types, names);
}
}  // namespace exec
}  // namespace carnot
}  // namespace pl
