#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "src/carnot/exec/table.h"
#include "src/common/status.h"
#include "src/stirling/data_table.h"
#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace carnot {
namespace exec {

using DefaultTableSchema = stirling::SeqGenConnector;

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
  Table* GetTable(const std::string& table_name) {
    return table_name_to_table_map_[table_name].get();
  }

  /*
   * Add a table under the given name.
   *
   * @ param table_name the name of the table to create.
   * @ param table the table to store.
   */
  void AddTable(const std::string& table_name, std::shared_ptr<Table> table) {
    table_name_to_table_map_.emplace(table_name, table);
  }

  /*
   * Add a table under the given name, with an assigned ID.
   *
   * @ param table_name the name of the table to create.
   * @ param table_id the unique ID of the table.
   * @ param table the table to store.
   */
  Status AddTable(const std::string& table_name, uint64_t table_id, std::shared_ptr<Table> table) {
    auto ok = table_id_to_table_map_.insert({table_id, table}).second;
    if (!ok) {
      return error::AlreadyExists("table_id=$0 is already in use");
    }

    AddTable(table_name, table);

    return Status::OK();
  }

  /**
   * Add a default table, for testing purposes.
   */
  // TODO(oazizi/anyone): Remove once pub-sub with Stirling is fleshed out.
  void AddDefaultTable();

  using ColNameToTypeMap = std::unordered_map<std::string, types::DataType>;
  /**
   * @return A map of table name to relation representing the table's structure.
   */
  std::shared_ptr<std::unordered_map<std::string, plan::Relation>> GetRelationMap();

  Status AppendData(uint64_t table_id,
                    std::unique_ptr<pl::stirling::ColumnWrapperRecordBatch> record_batch);

 private:
  std::unordered_map<std::string, std::shared_ptr<Table>> table_name_to_table_map_;
  std::unordered_map<uint64_t, std::shared_ptr<Table>> table_id_to_table_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
