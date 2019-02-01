#include <arrow/array.h>
#include <arrow/builder.h>
#include <glog/logging.h>

#include <algorithm>
#include <numeric>

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf.h"
#include "src/common/error.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {
namespace udf {

using arrow::Type;
udf::UDFDataType ArrowToCarnotType(const arrow::Type::type& arrow_type) {
  switch (arrow_type) {
    case Type::BOOL:
      return udf::UDFDataType::BOOLEAN;
    case Type::INT64:
      return udf::UDFDataType::INT64;
    case Type::DOUBLE:
      return udf::UDFDataType::FLOAT64;
    case Type::STRING:
      return udf::UDFDataType::STRING;
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
    BUILDER_CASE(udf::UDFDataType::BOOLEAN, mem_pool);
    BUILDER_CASE(udf::UDFDataType::INT64, mem_pool);
    BUILDER_CASE(udf::UDFDataType::FLOAT64, mem_pool);
    BUILDER_CASE(udf::UDFDataType::STRING, mem_pool);
    default:
      CHECK(0) << "Unknown data type: " << static_cast<int>(data_type);
  }
  return std::unique_ptr<arrow::ArrayBuilder>();
}

#undef BUILDER_CASE

}  // namespace udf
}  // namespace carnot
}  // namespace pl
