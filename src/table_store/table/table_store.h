/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/hash_utils.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/schema.h"
#include "src/table_store/table/table.h"
#include "src/table_store/table/tablets_group.h"

namespace px {
namespace table_store {

// NameTablet used as a key containing a Table name and Tablet ID.
struct NameTablet {
  bool operator==(const NameTablet& other) const {
    return name_ == other.name_ && tablet_id_ == other.tablet_id_;
  }

  template <typename H>
  friend H AbslHashValue(H h, const NameTablet& p) {
    return H::combine(std::move(h), p.name_, p.tablet_id_);
  }

  std::string name_;
  types::TabletID tablet_id_;
};

// TableIDTablet used as a key containing a Table ID and Tablet ID.
struct TableIDTablet {
  bool operator==(const TableIDTablet& other) const {
    return table_id_ == other.table_id_ && tablet_id_ == other.tablet_id_;
  }

  template <typename H>
  friend H AbslHashValue(H h, const TableIDTablet& p) {
    return H::combine(std::move(h), p.table_id_, p.tablet_id_);
  }

  uint64_t table_id_;
  types::TabletID tablet_id_;
};

// TableNameAndRelation contains a string name and a relation for a table.
struct TableInfo {
  std::string table_name;
  schema::Relation relation;
};

/**
 * TableStore keeps track of the tables in our system.
 */
class TableStore {
 public:
  using RelationMap = std::unordered_map<std::string, schema::Relation>;

  TableStore() = default;

  /**
   * Get table IDs returns a list of table ids available in the table store.
   * @return vector of table ids.
   */
  std::vector<uint64_t> GetTableIDs() const;

  /**
   * Gets the table associated with the given name, grabbing the default tablet.
   *
   * @ param table_name the name of the table to get
   * @ returns the associated table
   */
  table_store::Table* GetTable(const std::string& table_name,
                               const types::TabletID& tablet_id = kDefaultTablet) const;

  /**
   * @brief Get the Table according to table_id.
   *
   * @param table_id: the table_id to query.
   * @param tablet_id: the tablet_id to query upon.
   * @return table_store::Table*: the tablet associated with the table.
   */
  table_store::Table* GetTable(uint64_t table_id,
                               const types::TabletID& tablet_id = kDefaultTablet) const;

  /**
   * Add a table under the given name and optionally tablet id.
   *
   * @ param table: the table to store.
   * @ param table_name: the name of the table to add this under.
   * @ param table_id: optional unique ID to for the table name, for quick-lookups.
   * @ param tablet_id: the tablet id to assign this to.
   *
   * Unlike most interfaces, this has the value first (table), and the key second (table_name,
   * table_id and tablet_id are all essentially keys). This is done because to accommodate
   * optional fields.
   */
  void AddTable(std::shared_ptr<table_store::Table> table, const std::string& table_name,
                std::optional<uint64_t> table_id = std::nullopt,
                const types::TabletID& tablet_id = kDefaultTablet);

  // Old interface: Deprecated.
  void AddTable(const std::string& table_name, std::shared_ptr<table_store::Table> table) {
    return AddTable(std::move(table), table_name);
  }

  /**
   * Creates a mapping between a table ID and an existing table as specified by the name.
   * @return Error if table name does not exist.
   */
  Status AddTableAlias(uint64_t table_id, const std::string& table_name);

  /**
   * @return A map of table name to relation representing the table's structure.
   */
  std::unique_ptr<RelationMap> GetRelationMap();

  /**
   * @brief Appends the record_batch to the sepcified table and tablet_id. If the table exists but
   * the tablet does not, then the method creates a new container for the tablet.
   * If the table doesn't exist, then the method errors out.
   *
   * @param table_id: the id of the table to append to.
   * @param tablet_id: the tablet within the table to append to.
   * @param record_batch: the data to append.
   * @return Status: error if anything goes wrong during the process.
   */
  Status AppendData(uint64_t table_id, types::TabletID tablet_id,
                    std::unique_ptr<px::types::ColumnWrapperRecordBatch> record_batch);

  Status SchemaAsProto(schemapb::Schema* schema) const;

  /**
   * GetTableName returns the table name if the ID is found, else empty string.
   */
  std::string GetTableName(uint64_t id) const {
    const auto& it = id_to_table_info_map_.find(id);
    if (it != id_to_table_info_map_.end()) {
      return it->second.table_name;
    }
    return "";
  }

  Status RunCompaction(arrow::MemoryPool* mem_pool);

 private:
  void RegisterTableName(const std::string& table_name, const types::TabletID& tablet_id,
                         const schema::Relation& table_relation,
                         std::shared_ptr<table_store::Table> table);

  void RegisterTableID(uint64_t table_id, TableInfo table_info, const types::TabletID& tablet_id,
                       std::shared_ptr<table_store::Table> table);

  /**
   * Create a new tablet inside of the table with table_id
   *
   * @param table_id: table_id within which to create a tablet.
   * @param tablet_id: the tablet to create for the tablet.
   * @return StatusOr<Table*>: the table object or an error if the table is nonexistant.
   */
  StatusOr<Table*> CreateNewTablet(uint64_t table_id, const types::TabletID& tablet_id);

  // The default value for tablets, when tablet is not specified.
  inline static types::TabletID kDefaultTablet = "";
  // Map a name to a table.
  absl::flat_hash_map<NameTablet, std::shared_ptr<Table>> name_to_table_map_;
  // Map an id to a table.
  absl::flat_hash_map<TableIDTablet, std::shared_ptr<Table>> id_to_table_map_;
  // Mapping from name to relation for adding new tablets.
  // TODO(oazizi): value should likely be shared_ptr<schema::Relation> because the
  //               same information is in id_to_table_info_map_ TableInfo.
  //               Can avoid this copy.
  absl::flat_hash_map<std::string, schema::Relation> name_to_relation_map_;
  // Mapping from id to name and relation pair for adding new tablets.
  absl::flat_hash_map<uint64_t, TableInfo> id_to_table_info_map_;
};

}  // namespace table_store
}  // namespace px
