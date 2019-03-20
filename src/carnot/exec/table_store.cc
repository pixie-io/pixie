#include <utility>
#include <vector>

#include "src/carnot/exec/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

std::shared_ptr<std::unordered_map<std::string, plan::Relation>> TableStore::GetRelationMap() {
  auto map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
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

}  // namespace exec
}  // namespace carnot
}  // namespace pl
