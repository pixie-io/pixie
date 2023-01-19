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

#include <utility>

#include "src/vizier/services/agent/shared/manager/relation_info_manager.h"

namespace px {
namespace vizier {
namespace agent {

Status RelationInfoManager::AddRelationInfo(RelationInfo relation_info) {
  absl::base_internal::SpinLockHolder lock(&relation_info_map_lock_);
  if (relation_info_map_.contains(relation_info.name)) {
    return error::AlreadyExists("Relation '$0' already exists", relation_info.name);
  }
  std::string name = relation_info.name;
  relation_info_map_[name] = std::move(relation_info);
  has_updates_ = true;
  return Status::OK();
}

bool RelationInfoManager::HasRelation(std::string_view name) const {
  absl::base_internal::SpinLockHolder lock(&relation_info_map_lock_);
  return relation_info_map_.contains(name);
}

// TODO(philkuz) (PL-852) only send schema updates for changes to the schema.
void RelationInfoManager::AddSchemaToUpdateInfo(messages::AgentUpdateInfo* update_info) const {
  absl::base_internal::SpinLockHolder lock(&relation_info_map_lock_);

  update_info->set_does_update_schema(true);
  for (const auto& [name, relation_info] : relation_info_map_) {
    auto* schema = update_info->add_schema();
    schema->set_name(relation_info.name);
    schema->set_desc(relation_info.desc);
    const table_store::schema::Relation& relation = relation_info.relation;
    if (relation_info.tabletized) {
      schema->set_tabletized(relation_info.tabletized);
      schema->set_tabletization_key(relation.GetColumnName(relation_info.tabletization_key_idx));
    }
    for (size_t i = 0; i < relation.NumColumns(); ++i) {
      auto* column = schema->add_columns();
      column->set_name(relation.GetColumnName(i));
      column->set_data_type(relation.GetColumnType(i));
      column->set_desc(relation.GetColumnDesc(i));
      column->set_semantic_type(relation.GetColumnSemanticType(i));
      column->set_pattern_type(relation.GetColumnPatternType(i));
    }
  }
  has_updates_ = false;
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
