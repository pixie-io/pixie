#include <arrow/array.h>
#include <arrow/builder.h>

#include <algorithm>
#include <numeric>

#include <memory>
#include <string>
#include <vector>

#include <absl/numeric/int128.h>
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"

namespace pl {
namespace types {

using arrow::Type;
DataType ArrowToDataType(const arrow::Type::type& arrow_type) {
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES
  switch (arrow_type) {
    case Type::BOOL:
      return DataType::BOOLEAN;
    case Type::INT64:
      return DataType::INT64;
    case Type::UINT128:
      return DataType::UINT128;
    case Type::DOUBLE:
      return DataType::FLOAT64;
    case Type::STRING:
      return DataType::STRING;
    case Type::TIME64:
      return DataType::TIME64NS;
    default:
      CHECK(0) << "Unknown arrow data type: " << arrow_type;
  }
}

arrow::Type::type ToArrowType(const DataType& udf_type) {
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES
  switch (udf_type) {
    case DataType::BOOLEAN:
      return Type::BOOL;
    case DataType::INT64:
      return Type::INT64;
    case DataType::UINT128:
      return Type::UINT128;
    case DataType::FLOAT64:
      return Type::DOUBLE;
    case DataType::STRING:
      return Type::STRING;
    case DataType::TIME64NS:
      return Type::INT64;
    default:
      CHECK(0) << "Unknown udf data type: " << udf_type;
  }
}

int64_t ArrowTypeToBytes(const arrow::Type::type& arrow_type) {
  switch (arrow_type) {
    case Type::BOOL:
      return sizeof(bool);
    case Type::INT64:
      return sizeof(int64_t);
    case Type::UINT128:
      return sizeof(absl::uint128);
    case Type::FLOAT:
      return sizeof(float);
    case Type::TIME64:
      return sizeof(int64_t);
    case Type::DURATION:
      return sizeof(int64_t);
    case Type::DOUBLE:
      return sizeof(double);
    default:
      CHECK(0) << "Unknown arrow data type: " << arrow_type;
  }
}

#define BUILDER_CASE(__data_type__, __pool__) \
  case __data_type__:                         \
    return std::make_unique<DataTypeTraits<__data_type__>::arrow_builder_type>(__pool__)

std::unique_ptr<arrow::ArrayBuilder> MakeArrowBuilder(const DataType& data_type,
                                                      arrow::MemoryPool* mem_pool) {
  switch (data_type) {
    // PL_CARNOT_UPDATE_FOR_NEW_TYPES
    BUILDER_CASE(DataType::BOOLEAN, mem_pool);
    BUILDER_CASE(DataType::INT64, mem_pool);
    BUILDER_CASE(DataType::UINT128, mem_pool);
    BUILDER_CASE(DataType::FLOAT64, mem_pool);
    BUILDER_CASE(DataType::STRING, mem_pool);
    BUILDER_CASE(DataType::TIME64NS, mem_pool);
    default:
      CHECK(0) << "Unknown data type: " << static_cast<int>(data_type);
  }
  return std::unique_ptr<arrow::ArrayBuilder>();
}

#undef BUILDER_CASE

}  // namespace types
}  // namespace pl
