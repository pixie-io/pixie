#include <arrow/array.h>
#include <glog/logging.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/common/error.h"
#include "src/common/statusor.h"

namespace pl {
namespace carnot {
namespace exec {

Status Column::AddChunk(const std::shared_ptr<arrow::Array>& chunk) {
  // Check type and check size.
  if (udf::ArrowToCarnotType(chunk->type_id()) != data_type_) {
    return error::InvalidArgument("Column is of type $0, but needs to be type $1.",
                                  chunk->type_id(), data_type_);
  }

  chunks_.emplace_back(chunk);
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
    // Check number of chunks.
    if (columns_[0]->numChunks() != col->numChunks()) {
      return error::InvalidArgument("Column has $0 chunks, but should have $1.", col->numChunks(),
                                    columns_[0]->numChunks());
    }
    // Check size of chunks.
    for (int64_t chunk_idx = 0; chunk_idx < columns_[0]->numChunks(); chunk_idx++) {
      if (columns_[0]->chunk(chunk_idx)->length() != col->chunk(chunk_idx)->length()) {
        return error::InvalidArgument("Column has chunk of size $0, but should have size $1.",
                                      col->chunk(chunk_idx)->length(),
                                      columns_[0]->chunk(chunk_idx)->length());
      }
    }
  }

  columns_.emplace_back(col);
  name_to_column_map_.emplace(col->name(), col);
  return Status::OK();
}

StatusOr<std::unique_ptr<RowBatch>> Table::GetRowBatch(int64_t i, std::vector<int64_t> cols) {
  DCHECK(columns_.size() > 0) << "RowBatch does not have any columns.";
  DCHECK(columns_[0]->numChunks() > i) << absl::StrFormat(
      "Table has %d chunks, but requesting chunk %d", columns_[0]->numChunks(), i);

  std::vector<udf::UDFDataType> rb_types;
  for (auto col_idx : cols) {
    rb_types.push_back(desc_.type(col_idx));
  }
  auto output_rb =
      std::make_unique<RowBatch>(RowDescriptor(rb_types), columns_[0]->chunk(i)->length());
  for (auto col_idx : cols) {
    auto s = output_rb->AddColumn(columns_[col_idx]->chunk(i));
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
    auto s = columns_[i]->AddChunk(rb.ColumnAt(i));
    PL_RETURN_IF_ERROR(s);
  }
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
