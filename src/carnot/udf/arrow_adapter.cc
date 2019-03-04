#include <arrow/array.h>
#include <arrow/builder.h>

#include <algorithm>
#include <numeric>

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace udf {

using arrow::Type;
udf::UDFDataType ArrowToCarnotType(const arrow::Type::type& arrow_type) {
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES
  switch (arrow_type) {
    case Type::BOOL:
      return udf::UDFDataType::BOOLEAN;
    case Type::INT64:
      return udf::UDFDataType::INT64;
    case Type::DOUBLE:
      return udf::UDFDataType::FLOAT64;
    case Type::STRING:
      return udf::UDFDataType::STRING;
    case Type::TIME64:
      return udf::UDFDataType::TIME64NS;
    default:
      CHECK(0) << "Unknown arrow data type: " << arrow_type;
  }
}

arrow::Type::type CarnotToArrowType(const udf::UDFDataType& udf_type) {
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES
  switch (udf_type) {
    case udf::UDFDataType::BOOLEAN:
      return Type::BOOL;
    case udf::UDFDataType::INT64:
      return Type::INT64;
    case udf::UDFDataType::FLOAT64:
      return Type::DOUBLE;
    case udf::UDFDataType::STRING:
      return Type::STRING;
    case udf::UDFDataType::TIME64NS:
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
    case Type::FLOAT:
      return sizeof(float);
    case Type::TIME64:
      return sizeof(int64_t);
    case Type::DOUBLE:
      return sizeof(double);
    default:
      CHECK(0) << "Unknown arrow data type: " << arrow_type;
  }
}

#define BUILDER_CASE(__data_type__, __pool__) \
  case __data_type__:                         \
    return std::make_unique<udf::UDFDataTypeTraits<__data_type__>::arrow_builder_type>(__pool__)

std::unique_ptr<arrow::ArrayBuilder> MakeArrowBuilder(const udf::UDFDataType& data_type,
                                                      arrow::MemoryPool* mem_pool) {
  switch (data_type) {
    // PL_CARNOT_UPDATE_FOR_NEW_TYPES
    BUILDER_CASE(udf::UDFDataType::BOOLEAN, mem_pool);
    BUILDER_CASE(udf::UDFDataType::INT64, mem_pool);
    BUILDER_CASE(udf::UDFDataType::FLOAT64, mem_pool);
    BUILDER_CASE(udf::UDFDataType::STRING, mem_pool);
    BUILDER_CASE(udf::UDFDataType::TIME64NS, mem_pool);
    default:
      CHECK(0) << "Unknown data type: " << static_cast<int>(data_type);
  }
  return std::unique_ptr<arrow::ArrayBuilder>();
}

#undef BUILDER_CASE

}  // namespace udf
}  // namespace carnot
}  // namespace pl
