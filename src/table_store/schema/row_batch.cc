#include <arrow/array.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"
#include "src/table_store/schema/row_batch.h"

namespace pl {
namespace table_store {
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
  std::string debug_string = absl::StrFormat("RowBatch(eow=%d, eos=%d):\n", eow_, eos_);
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
#define TYPE_CASE(_dt_) total_bytes += types::GetArrowArrayBytes<_dt_>(col.get());
    PL_SWITCH_FOREACH_DATATYPE(types::ArrowToDataType(col->type_id()), TYPE_CASE);
#undef TYPE_CASE
  }
  return total_bytes;
}

}  // namespace schema
}  // namespace table_store
}  // namespace pl
