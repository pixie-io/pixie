#pragma once

#include <vector>

#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace exec {

/**
 * RowDescriptor describes the datatypes for each column in a RowBatch.
 */
class RowDescriptor {
 public:
  explicit RowDescriptor(const std::vector<udf::UDFDataType>& types) : types_(types) {}

  /**
   * Gets all the datatypes in the row descriptor.
   * @ return Vector of datatypes.
   */
  const std::vector<udf::UDFDataType>& types() const { return types_; }

  /**
   *  Gets the datatype for a specific column index.
   *  @ return the UDFDataType for the given column index.
   */
  udf::UDFDataType type(int64_t i) const { return types_[i]; }

  /**
   * @ return the number of columns that the row descriptor is describing.
   */
  size_t size() const { return types_.size(); }

 private:
  std::vector<udf::UDFDataType> types_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
