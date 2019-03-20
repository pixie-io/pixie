#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/common.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/proto/collector_config.pb.h"

namespace pl {
namespace stirling {

/**
 * DataTableSchema is simply an ordered list of DataElements that defines the schema of a
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
   * @return DataElement
   */
  DataElement operator[](size_t idx) const { return fields_[idx]; }

  /**
   * @brief Return a reference to element at the specified index.
   *
   * @return DataElement
   */
  DataElement& operator[](size_t idx) { return fields_[idx]; }

  /**
   * @brief Return the number of fields in the schema.
   *
   * @return uint64_t number of fields.
   */
  size_t NumFields() const { return fields_.size(); }

 private:
  std::vector<DataElement> fields_;
};

}  // namespace stirling
}  // namespace pl
