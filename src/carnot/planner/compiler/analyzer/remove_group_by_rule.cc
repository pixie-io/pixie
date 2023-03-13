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

#include "src/carnot/planner/compiler/analyzer/remove_group_by_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;

StatusOr<bool> RemoveGroupByRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GroupBy())) {
    return RemoveGroupBy(static_cast<GroupByIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> RemoveGroupByRule::RemoveGroupBy(GroupByIR* groupby) {
  if (groupby->Children().size() != 0) {
    return groupby->CreateIRNodeError(
        "'groupby()' should be followed by an 'agg()' or a 'rolling()' not a $0",
        groupby->Children()[0]->type_string());
  }
  auto graph = groupby->graph();
  auto groupby_id = groupby->id();
  auto groupby_children = graph->dag().DependenciesOf(groupby->id());
  PX_RETURN_IF_ERROR(graph->DeleteNode(groupby_id));
  for (const auto& child_id : groupby_children) {
    PX_RETURN_IF_ERROR(graph->DeleteOrphansInSubtree(child_id));
  }
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
