#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "src/carnot/exec/table.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {
namespace exec {

/**
 * TableStore keeps track of the tables in our system.
 */
class TableStore {
 public:
  TableStore() {}
  /*
   * Gets the table associated with the given name.
   *
   * @ param table_name the name of the table to get
   * @ returns the associated table
   */
  Table* GetTable(std::string table_name) { return table_name_to_table_map_[table_name].get(); }

  /*
   * Add a table under the given name.
   *
   * @ param table_name the name of the table to create.
   * @ param table the table to store.
   */
  void AddTable(std::string table_name, std::shared_ptr<Table> table) {
    table_name_to_table_map_.emplace(table_name, table);
  }

  using ColNameToTypeMap = std::unordered_map<std::string, udf::UDFDataType>;
  /**
   * @return A map of table name to a map of column name to column type.
   */
  std::unordered_map<std::string, ColNameToTypeMap> GetTableTypesLookup() {
    std::unordered_map<std::string, ColNameToTypeMap> map;
    map.reserve(table_name_to_table_map_.size());
    for (const auto& table : table_name_to_table_map_) {
      map.emplace(table.first, table.second->ColumnNameToTypeMap());
    }
    return map;
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<Table>> table_name_to_table_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
