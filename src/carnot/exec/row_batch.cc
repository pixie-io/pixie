#include <arrow/array.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/exec/row_batch.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf_wrapper.h"
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

int64_t RowBatch::NumBytes() const {
  if (num_rows() == 0) {
    return 0;
  }

  int64_t total_bytes = 0;
  for (auto col : columns_) {
    if (col->type_id() == arrow::Type::STRING) {
      // Loop through each string in the Arrow array.
      for (int64_t i = 0; i < col->length(); i++) {
        total_bytes += sizeof(char) *
                       udf::GetValueFromArrowArray<udf::UDFDataType::STRING>(col.get(), i).length();
      }
    } else {
      total_bytes += num_rows() * udf::ArrowTypeToBytes(col->type_id());
    }
  }
  return total_bytes;
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
