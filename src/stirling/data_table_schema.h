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

using stirlingpb::Element_State;
using types::DataType;

// TODO(oazizi): Using InfoClassElement as base class, because we want name and type.
// But we actually don't need State. Could consider refactoring with a common abstract base class.
class DataTableElement : public InfoClassElement {
 public:
  explicit DataTableElement(InfoClassElement e) : InfoClassElement(e) {}
  size_t offset() { return offset_; }
  void SetOffset(size_t offset) { offset_ = offset; }

  std::shared_ptr<arrow::DataType> arrow_type() { return types::DataTypeToArrowType(type()); }

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
  explicit DataTableSchema(const InfoClassSchema& info_class_schema);

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
