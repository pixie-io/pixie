#include <utility>
#include <vector>

#include "src/carnot/exec/table_store.h"
#include "src/carnot/schema/table.h"

namespace pl {
namespace carnot {
namespace exec {

std::unique_ptr<std::unordered_map<std::string, schema::Relation>> TableStore::GetRelationMap() {
  auto map = std::make_unique<RelationMap>();
  map->reserve(table_name_to_table_map_.size());
  for (const auto& table : table_name_to_table_map_) {
    map->emplace(table.first, table.second->GetRelation());
  }
  return map;
}

Status TableStore::AppendData(uint64_t table_id,
                              std::unique_ptr<pl::types::ColumnWrapperRecordBatch> record_batch) {
  auto table = table_id_to_table_map_[table_id];
  PL_RETURN_IF_ERROR(table->TransferRecordBatch(std::move(record_batch)));
  return Status::OK();
}

schema::Table* TableStore::GetTable(const std::string& table_name) {
  return table_name_to_table_map_[table_name].get();
}

void TableStore::AddTable(const std::string& table_name, std::shared_ptr<schema::Table> table) {
  table_name_to_table_map_.emplace(table_name, table);
}

Status TableStore::AddTable(const std::string& table_name, uint64_t table_id,
                            std::shared_ptr<schema::Table> table) {
  auto ok = table_id_to_table_map_.insert({table_id, table}).second;
  if (!ok) {
    return error::AlreadyExists("table_id=$0 is already in use");
  }

  AddTable(table_name, table);

  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
