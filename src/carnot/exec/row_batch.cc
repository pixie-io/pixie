#include <arrow/array.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/exec/row_batch.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace exec {

std::shared_ptr<arrow::Array> RowBatch::ColumnAt(int64_t i) const { return columns_[i]; }

Status RowBatch::AddColumn(const std::shared_ptr<arrow::Array>& col) {
  if (columns_.size() >= desc_.size()) {
    return error::InvalidArgument("Schema only allows %d columns", desc_.size());
  }
  if (col->length() != num_rows_) {
    return error::InvalidArgument("Schema only allows %d rows", num_rows_);
  }
  if (col->type_id() != udf::CarnotToArrowType(desc_.type(columns_.size()))) {
    return error::InvalidArgument("Column[%d] was given incorrect type", columns_.size());
  }
  columns_.emplace_back(col);
  return Status::OK();
}

bool RowBatch::HasColumn(int64_t i) const { return columns_.size() > static_cast<size_t>(i); }

std::string RowBatch::DebugString() const {
  if (columns_.empty()) {
    return "RowBatch: <empty>";
  }
  std::string debug_string = "RowBatch:\n";
  for (const auto& col : columns_) {
    debug_string += absl::StrFormat("  %s\n", col->ToString());
  }
  return debug_string;
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
