#include <memory>
#include <utility>
#include <vector>

#include "src/common/macros.h"
#include "src/stirling/data_table_schema.h"

namespace pl {
namespace stirling {

// Generate the appropriate data table schema from the InfoClassSchema.
DataTableSchema::DataTableSchema(const InfoClassSchema& schema) {
  for (const auto& element : schema) {
    // Create DataElement for subscribed elements
    if (element.state() == stirlingpb::Element_State::Element_State_SUBSCRIBED) {
      fields_.emplace_back(element);
    }
  }
}

}  // namespace stirling
}  // namespace pl
