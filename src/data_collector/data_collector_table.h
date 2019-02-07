#pragma once

#include <memory>
#include <vector>

#include "src/data_collector/data_table_schema.h"
#include "src/data_collector/info_class_schema.h"
#include "third_party/arrow/cpp/src/arrow/type.h"

namespace pl {
namespace datacollector {

class DataTable {
 public:
  DataTable() = delete;
  ~DataTable() = default;
  explicit DataTable(const InfoClassSchema& schema);

  /**
   * Given an InfoClassSchema, generate the appropriate table.
   */
  void RegisterTable(const InfoClassSchema& schema);

  /**
   * Given raw data and a schema, append the data to the existing Data Tables.
   */
  void AppendData(char* data, uint64_t num_rows);

 private:
  std::unique_ptr<DataTableSchema> table_schema_;
  std::vector<uint32_t> offsets_;
  uint32_t row_size_;
};

}  // namespace datacollector
}  // namespace pl
