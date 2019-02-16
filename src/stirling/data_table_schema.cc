#include <memory>
#include <utility>
#include <vector>

#include "src/common/macros.h"
#include "src/stirling/data_table_schema.h"

namespace pl {
namespace stirling {

// Generate the appropriate data table schema from the InfoClassSchema.
DataTableSchema::DataTableSchema(const InfoClassSchema& schema) {
  for (uint32_t i = 0; i < schema.NumElements(); ++i) {
    const auto& element = schema.GetElement(i);

    // TODO(oazizi): Rethink the states that should be collected.
    if (element.state() == Element_State::Element_State_COLLECTED_AND_SUBSCRIBED ||
        element.state() == Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED) {
      fields_.push_back(DataTableElement(element));
    }
  }
}

}  // namespace stirling
}  // namespace pl
