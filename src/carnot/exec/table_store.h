#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "src/common/status.h"
#include "src/shared/types/column_wrapper.h"

namespace pl {
namespace carnot {
namespace plan {

// Forward declare to break include dependency.
// TODO(zasgar): remove this after moving relation/table to external API.
class Relation;
}  // namespace  plan

namespace exec {

// Forward declare to break include depedency.
class Table;

/**
 * TableStore keeps track of the tables in our system.
 */
class TableStore {
 public:
  using ColNameToTypeMap = std::unordered_map<std::string, types::DataType>;

  TableStore() = default;
  /*
   * Gets the table associated with the given name.
   *
   * @ param table_name the name of the table to get
   * @ returns the associated table
   */
  Table* GetTable(const std::string& table_name);

  /*
   * Add a table under the given name.
   *
   * @ param table_name the name of the table to create.
   * @ param table the table to store.
   */
  void AddTable(const std::string& table_name, std::shared_ptr<Table> table);

  /*
   * Add a table under the given name, with an assigned ID.
   *
   * @ param table_name the name of the table to create.
   * @ param table_id the unique ID of the table.
   * @ param table the table to store.
   */
  Status AddTable(const std::string& table_name, uint64_t table_id, std::shared_ptr<Table> table);

  /**
   * @return A map of table name to relation representing the table's structure.
   */
  std::shared_ptr<std::unordered_map<std::string, plan::Relation>> GetRelationMap();

  Status AppendData(uint64_t table_id,
                    std::unique_ptr<pl::types::ColumnWrapperRecordBatch> record_batch);

 private:
  std::unordered_map<std::string, std::shared_ptr<Table>> table_name_to_table_map_;
  std::unordered_map<uint64_t, std::shared_ptr<Table>> table_id_to_table_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
