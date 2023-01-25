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

#include "src/carnot/planner/ir/rolling_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {

Status RollingIR::Init(OperatorIR* parent, ColumnIR* window_col, int64_t window_size) {
  PX_RETURN_IF_ERROR(AddParent(parent));
  PX_RETURN_IF_ERROR(SetWindowCol(window_col));
  window_size_ = window_size;
  return Status::OK();
}

Status RollingIR::SetWindowCol(ColumnIR* window_col) {
  PX_ASSIGN_OR_RETURN(window_col_, graph()->OptionallyCloneWithEdge(this, window_col));
  return Status::OK();
}

Status RollingIR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const RollingIR* rolling_node = static_cast<const RollingIR*>(source);
  PX_ASSIGN_OR_RETURN(IRNode * new_window_col,
                      graph()->CopyNode(rolling_node->window_col(), copied_nodes_map));
  DCHECK(Match(new_window_col, ColumnNode()));
  PX_RETURN_IF_ERROR(SetWindowCol(static_cast<ColumnIR*>(new_window_col)));
  window_size_ = rolling_node->window_size();
  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : rolling_node->groups()) {
    PX_ASSIGN_OR_RETURN(ColumnIR * new_column, graph()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }
  return SetGroups(new_groups);
}

Status RollingIR::ToProto(planpb::Operator* /* op */) const {
  return CreateIRNodeError("Rolling operator not yet implemented");
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> RollingIR::RequiredInputColumns() const {
  return error::Unimplemented("Rolling operator doesn't support RequiredInputColumns.");
}

StatusOr<absl::flat_hash_set<std::string>> RollingIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& /* output_colnames */) {
  return error::Unimplemented("Rolling operator doesn't support PruneOutputColumntTo.");
}
}  // namespace planner
}  // namespace carnot
}  // namespace px
