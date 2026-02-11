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

#include "src/table_store/table/tablets_group.h"

namespace px {
namespace table_store {

void TabletsGroup::CreateTablet(const types::TabletID& tablet_id) {
  LOG_IF(DFATAL, HasTablet(tablet_id))
      << absl::Substitute("Tablet with id $0 already exists in Table.", tablet_id);
  tablet_id_to_tablet_map_[tablet_id] = Table::Create(tablet_id, relation_);
}

void TabletsGroup::AddTablet(const types::TabletID& tablet_id, std::shared_ptr<Table> tablet) {
  LOG_IF(DFATAL, HasTablet(tablet_id))
      << absl::Substitute("Tablet with id $0 already exists in Table.", tablet_id);
  DCHECK_EQ(tablet->GetRelation(), relation_);
  tablet_id_to_tablet_map_[tablet_id] = tablet;
}

table_store::Table* TabletsGroup::GetTablet(const types::TabletID& tablet_id) const {
  auto tablet_id_to_tablet_map_iter = tablet_id_to_tablet_map_.find(tablet_id);
  if (tablet_id_to_tablet_map_iter == tablet_id_to_tablet_map_.end()) {
    return nullptr;
  }
  return tablet_id_to_tablet_map_iter->second.get();
}

bool TabletsGroup::HasTablet(const types::TabletID& tablet_id) const {
  return tablet_id_to_tablet_map_.find(tablet_id) != tablet_id_to_tablet_map_.end();
}

}  // namespace table_store
}  // namespace px
