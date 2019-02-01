#pragma once

#include <arrow/array.h>
#include <arrow/memory_pool.h>
#include <glog/logging.h>
#include <algorithm>
#include <numeric>
#include <string>

#include <memory>
#include <vector>

#include "src/carnot/udf/udf.h"
#include "src/common/error.h"
#include "src/common/status.h"

namespace pl {

/**
 * This functions adapts arrow Status and converts it to pl::Status.
 * TODO(zasgar): PL-276 - We should make this part of the status constructor defined
 * out-of-line.
 */
inline Status StatusAdapter(const arrow::Status& s) {
  if (s.ok()) {
    return Status::OK();
  }
  // TODO(zasgar): PL-276 - Finish mapping.
  return error::Unknown(s.message());
}

namespace carnot {
namespace udf {

// The functions convert vector of UDF values to an arrow representation on
// the given MemoryPool.
template <typename TUDFValue>
inline std::shared_ptr<arrow::Array> ToArrow(const std::vector<TUDFValue>& data,
                                             arrow::MemoryPool* mem_pool) {
  DCHECK(mem_pool != nullptr);

  typename UDFValueTraits<TUDFValue>::arrow_builder_type builder(mem_pool);
  PL_CHECK_OK(builder.Reserve(data.size()));
  for (const auto v : data) {
    builder.UnsafeAppend(v.val);
  }
  std::shared_ptr<arrow::Array> arr;
  PL_CHECK_OK(builder.Finish(&arr));
  return arr;
}

// Specialization of the above for strings.
template <>
inline std::shared_ptr<arrow::Array> ToArrow<StringValue>(const std::vector<StringValue>& data,
                                                          arrow::MemoryPool* mem_pool) {
  DCHECK(mem_pool != nullptr);
  arrow::StringBuilder builder(mem_pool);
  size_t total_size =
      std::accumulate(data.begin(), data.end(), 0ULL,
                      [](uint64_t sum, const std::string& str) { return sum + str.size(); });
  // This allocates space for null/ptrs/size.
  PL_CHECK_OK(builder.Reserve(data.size()));
  // This allocates space for the actual data.
  PL_CHECK_OK(builder.ReserveData(total_size));
  for (const auto val : data) {
    builder.UnsafeAppend(val);
  }
  std::shared_ptr<arrow::Array> arr;
  PL_CHECK_OK(builder.Finish(&arr));
  return arr;
}

/**
 * Find the carnot UDFDataType for a given arrow type.
 * @param arrow_type The arrow type.
 * @return The carnot UDFDataType.
 */
udf::UDFDataType ArrowToCarnotType(const arrow::Type::type& arrow_type);

/**
 * Make an arrow builder based on carnot UDFDataType and usng the passed in MemoryPool.
 * @param data_type The carnot UDFDataType.
 * @param mem_pool The MemoryPool to use.
 * @return a unique_ptr to an array builder.
 */
std::unique_ptr<arrow::ArrayBuilder> MakeArrowBuilder(const udf::UDFDataType& data_type,
                                                      arrow::MemoryPool* mem_pool);
}  // namespace udf
}  // namespace carnot
}  // namespace pl
