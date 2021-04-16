#pragma once

#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include "../../shared/types/types.h"

namespace px {
namespace table_store {
namespace schema {

/**
 * RowDescriptor describes the datatypes for each column in a RowBatch.
 */
class RowDescriptor {
 public:
  explicit RowDescriptor(std::vector<types::DataType> types) : types_(std::move(types)) {}

  /**
   * Gets all the datatypes in the row descriptor.
   * @ return Vector of datatypes.
   */
  const std::vector<types::DataType>& types() const { return types_; }
  std::vector<types::DataType>& types() { return types_; }

  /**
   *  Gets the datatype for a specific column index.
   *  @ return the UDFDataType for the given column index.
   */
  types::DataType type(int64_t i) const { return types_[i]; }

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

  bool operator==(const RowDescriptor& other) const { return other.types_ == types_; }

 private:
  std::vector<types::DataType> types_;
};

}  // namespace schema
}  // namespace table_store
}  // namespace px
