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

#include "src/carnot/planner/ir/group_by_ir.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

Status GroupByIR::Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups) {
  PX_RETURN_IF_ERROR(AddParent(parent));
  return SetGroups(groups);
}

Status GroupByIR::SetGroups(const std::vector<ColumnIR*>& groups) {
  DCHECK(groups_.empty());
  groups_.resize(groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    PX_ASSIGN_OR_RETURN(groups_[i], graph()->OptionallyCloneWithEdge(this, groups[i]));
  }
  return Status::OK();
}

Status GroupByIR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const GroupByIR* group_by = static_cast<const GroupByIR*>(source);
  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : group_by->groups_) {
    PX_ASSIGN_OR_RETURN(ColumnIR * new_column, graph()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }
  return SetGroups(new_groups);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
