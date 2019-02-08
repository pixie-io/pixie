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

 private:
  std::unordered_map<std::string, std::shared_ptr<Table>> table_name_to_table_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
