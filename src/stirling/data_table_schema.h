#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/common.h"
#include "src/common/types/types.pb.h"
#include "src/stirling/info_class_schema.h"
#include "src/stirling/proto/collector_config.pb.h"

namespace pl {
namespace stirling {

class DataTableElement : public DataElement {
 public:
  // Conversion constructor: Construct a DataTableElement from an InfoClassElement
  // Since they both have the same DataElement ancestor, we can simply call the
  // copy constructor DataElement.
  explicit DataTableElement(const InfoClassElement& e) : DataElement(e) {}
  size_t offset() { return offset_; }
  void SetOffset(size_t offset) { offset_ = offset; }

 private:
  size_t offset_ = 0;
};

/**
 * DataTableSchema is simply an ordered list of DataTableElements that defines the schema of a
 * DataTable.
 */
class DataTableSchema {
 public:
  /**
   * @brief Construct a new DataTableSchema from an existing InfoClassSchema.
   * Picks only elements in the correct state.
   */
  explicit DataTableSchema(const InfoClassSchema& schema);

  /**
   * @brief Return the element at the specified index. Typically used to get the type or name.
   *
   * @return DataTableElement
   */
  DataTableElement operator[](size_t idx) const { return fields_[idx]; }

  /**
   * @brief Return a reference to element at the specified index.
   *
   * @return DataTableElement
   */
  DataTableElement& operator[](size_t idx) { return fields_[idx]; }

  /**
   * @brief Return the number of fields in the schema.
   *
   * @return uint64_t number of fields.
   */
  size_t NumFields() const { return fields_.size(); }

 private:
  std::vector<DataTableElement> fields_;
};

}  // namespace stirling
}  // namespace pl
