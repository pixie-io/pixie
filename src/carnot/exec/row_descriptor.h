#pragma once

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
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

  /**
   * @return the debug string for the row descriptor.
   */
  std::string DebugString() const {
    std::string debug_string = "RowDescriptor:\n";
    for (const auto& type : types_) {
      debug_string += absl::StrFormat("  %d\n", type);
    }
    return debug_string;
  }

 private:
  std::vector<udf::UDFDataType> types_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
