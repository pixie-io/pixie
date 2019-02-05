#include <vector>

#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/data_collector/data_collector_table.h"

namespace pl {
namespace datacollector {

DataTable::DataTable(const InfoClassSchema& schema) { RegisterTable(schema); }

// Given an InfoClassSchema, generate the appropriate table.
void DataTable::RegisterTable(const InfoClassSchema& schema) {
  table_schema_ = schema.CreateDataTableSchema();

  size_t current_offset = 0;
  for (uint32_t i = 0; i < schema.NumElements(); ++i) {
    auto elementInfo = schema.GetElement(i);

    if (elementInfo.state() == Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED ||
        elementInfo.state() == Element_State::Element_State_COLLECTED_AND_SUBSCRIBED) {
      offsets_.push_back(current_offset);

      current_offset += elementInfo.WidthBytes();
    }
  }

  row_size_ = current_offset;
}

// Given raw data and a schema, append the data to the existing Data Tables.
void DataTable::AppendData(char* data, uint64_t num_rows) {
  std::vector<uint64_t> column_uint64(num_rows);
  std::vector<float> column_float(num_rows);

  for (uint32_t row = 0; row < num_rows; ++row) {
    for (int32_t i = 0; i < table_schema_->num_fields(); ++i) {
      char* element_ptr = static_cast<char*>(data + (row * row_size_) + offsets_[i]);
      PL_UNUSED(element_ptr);
      switch (table_schema_->field(i)->type()->id()) {
        case arrow::Type::type::INT64: {
          auto* val_ptr = reinterpret_cast<uint64_t*>(element_ptr);
          column_uint64.push_back(*val_ptr);
        } break;
        case arrow::Type::type::DOUBLE: {
          auto* val_ptr = reinterpret_cast<float*>(element_ptr);
          column_float.push_back(*val_ptr);
        } break;
        default:
          CHECK(0) << "Unimplemented data type";
      }
    }
  }
}

}  // namespace datacollector
}  // namespace pl
