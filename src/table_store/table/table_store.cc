#include <algorithm>
#include <utility>
#include <vector>

#include "src/table_store/table/table_store.h"

namespace pl {
namespace table_store {

std::unique_ptr<std::unordered_map<std::string, schema::Relation>> TableStore::GetRelationMap() {
  auto map = std::make_unique<RelationMap>();
  map->reserve(name_to_relation_map_.size());
  for (auto& [table_name, relation] : name_to_relation_map_) {
    map->emplace(table_name, relation);
  }
  return map;
}

StatusOr<Table*> TableStore::CreateNewTablet(uint64_t table_id, const types::TabletID& tablet_id) {
  auto id_to_table_info_map_iter = id_to_table_info_map_.find(table_id);
  if (id_to_table_info_map_iter == id_to_table_info_map_.end()) {
    return error::InvalidArgument("Table_id $0 doesn't exist.", table_id);
  }

  const TableInfo& table_info = id_to_table_info_map_iter->second;
  const schema::Relation& relation = table_info.relation;
  std::shared_ptr<Table> new_tablet = Table::Create(relation);

  TableIDTablet id_key = {table_id, tablet_id};
  id_to_table_map_[id_key] = new_tablet;

  const std::string& table_name = table_info.table_name;
  DCHECK(relation == name_to_relation_map_.find(table_name)->second);
  NameTablet name_key = {table_name, tablet_id};
  name_to_table_map_[name_key] = new_tablet;
  return new_tablet.get();
}

Status TableStore::AppendData(uint64_t table_id, types::TabletID tablet_id,
                              std::unique_ptr<pl::types::ColumnWrapperRecordBatch> record_batch) {
  Table* table = GetTable(table_id, tablet_id);
  // We create new tablets only if the table at `table_id` exists, otherwise errors out.
  if (table == nullptr) {
    PL_ASSIGN_OR_RETURN(table, CreateNewTablet(table_id, tablet_id));
  }
  return table->TransferRecordBatch(std::move(record_batch));
}

table_store::Table* TableStore::GetTable(const std::string& table_name,
                                         const types::TabletID& tablet_id) const {
  auto name_to_table_iter = name_to_table_map_.find(NameTablet{table_name, tablet_id});
  if (name_to_table_iter == name_to_table_map_.end()) {
    return nullptr;
  }
  return name_to_table_iter->second.get();
}

table_store::Table* TableStore::GetTable(uint64_t table_id,
                                         const types::TabletID& tablet_id) const {
  auto id_to_table_iter = id_to_table_map_.find(TableIDTablet{table_id, tablet_id});
  if (id_to_table_iter == id_to_table_map_.end()) {
    return nullptr;
  }
  return id_to_table_iter->second.get();
}

void TableStore::AddTableImpl(std::optional<uint64_t> table_id, const std::string& table_name,
                              const types::TabletID& tablet_id,
                              std::shared_ptr<table_store::Table> table) {
  const auto& table_relation = table->GetRelation();

  // Register the table by name.
  {
    auto name_to_relation_map_iter = name_to_relation_map_.find(table_name);
    if (name_to_relation_map_iter == name_to_relation_map_.end()) {
      name_to_relation_map_[table_name] = table_relation;
    } else {
      DCHECK_EQ(name_to_relation_map_iter->second, table_relation);
    }

    NameTablet key = {table_name, tablet_id};
    name_to_table_map_[key] = table;
  }

  // Register the table by ID, if one is present.
  if (table_id.has_value()) {
    // Lookup whether the table already exists in the relation map, add if it does not.
    auto id_to_table_info_map_iter = id_to_table_info_map_.find(table_id.value());
    if (id_to_table_info_map_iter == id_to_table_info_map_.end()) {
      id_to_table_info_map_[table_id.value()] = TableInfo({table_name, table_relation});
    } else {
      DCHECK_EQ(id_to_table_info_map_iter->second.relation, table_relation);
    }

    TableIDTablet key = {table_id.value(), tablet_id};
    id_to_table_map_[key] = table;
  }
}

Status TableStore::SchemaAsProto(schemapb::Schema* schema) const {
  return schema::Schema::ToProto(schema, name_to_relation_map_);
}

std::vector<uint64_t> TableStore::GetTableIDs() const {
  std::vector<uint64_t> ids;
  for (const auto& it : id_to_table_map_) {
    ids.emplace_back(it.first.table_id_);
  }
  return ids;
}

}  // namespace table_store
}  // namespace pl
