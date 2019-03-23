#include <arrow/array.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/schema/row_batch.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace schema {

std::shared_ptr<arrow::Array> RowBatch::ColumnAt(int64_t i) const { return columns_[i]; }

Status RowBatch::AddColumn(const std::shared_ptr<arrow::Array>& col) {
  if (columns_.size() >= desc_.size()) {
    return error::InvalidArgument("Schema only allows $0 columns", desc_.size());
  }
  if (col->length() != num_rows_) {
    return error::InvalidArgument("Schema only allows $0 rows, got $1", num_rows_, col->length());
  }
  if (col->type_id() != types::ToArrowType(desc_.type(columns_.size()))) {
    return error::InvalidArgument("Column[$0] was given incorrect type", columns_.size());
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
        total_bytes +=
            sizeof(char) *
            types::GetValueFromArrowArray<types::DataType::STRING>(col.get(), i).length();
      }
    } else {
      total_bytes += num_rows() * types::ArrowTypeToBytes(col->type_id());
    }
  }
  return total_bytes;
}

}  // namespace schema
}  // namespace carnot
}  // namespace pl
