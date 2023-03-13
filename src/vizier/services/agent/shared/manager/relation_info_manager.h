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
#include <string>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include <absl/container/btree_map.h>

#include "src/shared/schema/utils.h"
#include "src/vizier/messages/messagespb/messages.pb.h"

namespace px {
namespace vizier {
namespace agent {
/**
 * @brief Manager of relation info for a given agent. In the future, this can be used to provide
 * diff-updates of a schema rather than passing the entire schema through the update message.
 */
class RelationInfoManager {
 public:
  /**
   * @brief Adds the relation info to the agent state. Conflicting relation will
   * will be rejected with error.
   *
   * @param relation_info: The new relation to add.
   * @return Status: Error if relation is a conflict.
   */
  Status AddRelationInfo(RelationInfo relation_info);

  /**
   * Checks to see if a relation with the given name exists.
   * @param name The name of the relation.
   * @return true if it exists.
   */
  bool HasRelation(std::string_view name) const;

  /**
   * @brief Adds schema updates to the update_info message.
   *
   * Updates the has_update state.
   *
   * @param update_info: the message that should receive the updated schema info.
   */
  void AddSchemaToUpdateInfo(messages::AgentUpdateInfo* update_info) const;

  bool has_updates() const { return has_updates_; }

 private:
  mutable std::atomic<bool> has_updates_ = false;
  mutable absl::base_internal::SpinLock relation_info_map_lock_;
  absl::btree_map<std::string, RelationInfo> relation_info_map_ GUARDED_BY(relation_info_map_lock_);
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
