#include <vector>

#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/data_collector/data_collector_table.h"

namespace pl {
namespace datacollector {

DataTable::DataTable(const InfoClassSchema& schema) { RegisterTable(schema); }

// Given an InfoClassSchema, generate the appropriate table.
void DataTable::RegisterTable(const InfoClassSchema& schema) {
  table_schema_ = std::make_unique<DataTableSchema>(schema);

  size_t current_offset = 0;
  for (uint32_t i = 0; i < table_schema_->NumFields(); ++i) {
    auto& elementInfo = (*table_schema_)[i];
    elementInfo.SetOffset(current_offset);
    offsets_.push_back(current_offset);
    current_offset += elementInfo.WidthBytes();
  }

  row_size_ = current_offset;
}

// Given raw data and a schema, append the data to the existing Data Tables.
void DataTable::AppendData(char* data, uint64_t num_rows) {
  // TODO(oazizi): Implement this function. Current implementation is a placeholder.
  for (uint32_t row = 0; row < num_rows; ++row) {
    for (uint32_t i = 0; i < table_schema_->NumFields(); ++i) {
      char* element_ptr = static_cast<char*>(data + (row * row_size_) + offsets_[i]);
      PL_UNUSED(element_ptr);
      switch ((*table_schema_)[i].type()) {
        case DataType::INT64: {
          auto* val_ptr = reinterpret_cast<uint64_t*>(element_ptr);
          PL_UNUSED(val_ptr);
        } break;
        case DataType::FLOAT64: {
          auto* val_ptr = reinterpret_cast<double*>(element_ptr);
          PL_UNUSED(val_ptr);
        } break;
        default:
          CHECK(0) << "Unimplemented data type";
      }
    }
  }
}

}  // namespace datacollector
}  // namespace pl
