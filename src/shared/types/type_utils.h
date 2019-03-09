#pragma once

#include "third_party/arrow/cpp/src/arrow/type.h"

#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "src/common/common.h"
#include "src/shared/types/proto/types.pb.h"

namespace pl {
namespace types {

inline std::string ToString(DataType type) {
  switch (type) {
    case DataType::BOOLEAN:
      return "bool";
    case DataType::INT64:
      return "int64";
    case DataType::FLOAT64:
      return "float64";
    case DataType::STRING:
      return "string";
    case DataType::TIME64NS:
      return "time64ns";
    default:
      DCHECK(false) << "No ToString() for this DataType";
      return "UNKNOWN";
  }
}

inline size_t DataTypeWidthBytes(DataType type) {
  switch (type) {
    case DataType::FLOAT64:
      return (sizeof(double));
    case DataType::INT64:
      return (sizeof(int64_t));
    case DataType::TIME64NS:
      return (sizeof(int64_t));
    default:
      DCHECK(false) << absl::StrFormat("Unknown data type %s", ToString(type));
      return 0;
  }
}

inline std::shared_ptr<arrow::DataType> DataTypeToArrowType(DataType type) {
  switch (type) {
    case DataType::INT64:
      return arrow::int64();
    case DataType::FLOAT64:
      return arrow::float64();
    case DataType::TIME64NS:
      return arrow::time64(arrow::TimeUnit::NANO);
    default:
      DCHECK(false) << absl::StrFormat("Unknown data type %s", ToString(type));
      return 0;
  }
}

}  // namespace types
}  // namespace pl
