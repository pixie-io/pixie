#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

/**
 * TableStore keeps track of the tables in our system.
 */
class TableStore {
 public:
  using RelationMap = std::unordered_map<std::string, table_store::schema::Relation>;
  using ColNameToTypeMap = std::unordered_map<std::string, types::DataType>;

  TableStore() = default;
  /*
   * Gets the table associated with the given name.
   *
   * @ param table_name the name of the table to get
   * @ returns the associated table
   */
  table_store::schema::Table* GetTable(const std::string& table_name);

  /*
   * Add a table under the given name.
   *
   * @ param table_name the name of the table to create.
   * @ param table the table to store.
   */
  void AddTable(const std::string& table_name, std::shared_ptr<table_store::schema::Table> table);

  /*
   * Add a table under the given name, with an assigned ID.
   *
   * @ param table_name the name of the table to create.
   * @ param table_id the unique ID of the table.
   * @ param table the table to store.
   */
  Status AddTable(const std::string& table_name, uint64_t table_id,
                  std::shared_ptr<table_store::schema::Table> table);

  /**
   * @return A map of table name to relation representing the table's structure.
   */
  std::unique_ptr<RelationMap> GetRelationMap();

  Status AppendData(uint64_t table_id,
                    std::unique_ptr<pl::types::ColumnWrapperRecordBatch> record_batch);

 private:
  std::unordered_map<std::string, std::shared_ptr<table_store::schema::Table>>
      table_name_to_table_map_;
  std::unordered_map<uint64_t, std::shared_ptr<table_store::schema::Table>> table_id_to_table_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
