#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/proto/collector_config.pb.h"

namespace pl {
namespace stirling {

/**
 * DataTableSchema is simply an ordered list of DataElement that defines the schema of a
 * DataTable.
 */
// TODO(yzhao): Is this class necessary? Seems replaceable with a vanilla std::vector<DataElement>.
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

  /**
   * @brief Returns a reference to the underlying data.
   *
   * @return const std::vector<DataElement>& the underlying data.
   */
  const std::vector<DataElement>& AsVector() const { return fields_; }

 private:
  std::vector<DataElement> fields_;
};

}  // namespace stirling
}  // namespace pl
