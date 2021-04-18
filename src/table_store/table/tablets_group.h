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

#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/table/table.h"

namespace px {
namespace table_store {

/**
 * @brief The container of tablets.
 *
 *Tables are currently the data structure of tablets, so we should do the following refactoring.
 * `s/table_store::Table/table_store::Tablet/g` and
 *`s/table_store::TabletsGroup/table_store::Table/g`
 * TODO(philkuz) refactor as stated above.
 */
class TabletsGroup {
 public:
  explicit TabletsGroup(const schema::Relation& relation) : relation_(relation) {}
  /**
   * @brief Create a Tablet inside of this group, with the given tablet_id and the relation
   * of this TabletGroup.
   *
   * @param tablet_id
   */
  void CreateTablet(const types::TabletID& tablet_id);

  /**
   * @brief Adding a preconfigured tablet to the tablet group with the given tablet_id.
   *
   * @param tablet_id
   * @param tablet: the tablet created elsewhere that must match the relation of this TabletsGroup.
   */
  void AddTablet(const types::TabletID& tablet_id, std::shared_ptr<Table> tablet);

  /**
   * @brief Get the Tablet with the id.
   *
   * @param tablet_id
   * @return table_store::Table* if found, otherwise nullptr.
   */
  table_store::Table* GetTablet(const types::TabletID& tablet_id) const;

  /**
   * @brief Returns true whether this Tablet Group has a tablet.
   *
   * @param tablet_id: id of the tablet.
   * @return true: if tablet exists.
   * @return false: if the tablet does not exist.
   */
  bool HasTablet(const types::TabletID& tablet_id) const;

  /**
   * @brief Gets the relation for the tablets in this TabletsGroup.
   *
   * @return const schema::Relation&
   */
  const schema::Relation& GetRelation() const { return relation_; }

 private:
  schema::Relation relation_;
  std::unordered_map<types::TabletID, std::shared_ptr<Table>> tablet_id_to_tablet_map_;
};

}  // namespace table_store
}  // namespace px
