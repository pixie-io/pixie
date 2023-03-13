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

#include "src/carnot/planner/compiler/optimizer/prune_unconnected_operators_rule.h"

#include <algorithm>
#include <queue>

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> PruneUnconnectedOperatorsRule::Apply(IRNode* ir_node) {
  auto ir_graph = ir_node->graph();
  auto node_id = ir_node->id();

  if (Match(ir_node, ResultSink()) || sink_connected_nodes_.contains(ir_node)) {
    for (int64_t parent_id : ir_graph->dag().ParentsOf(node_id)) {
      sink_connected_nodes_.insert(ir_graph->Get(parent_id));
    }
    return false;
  }
  if (!Match(ir_node, Operator())) return false;
  std::vector<int64_t> nodes_to_remove;
  nodes_to_remove.push_back(node_id);

  // Remove child IR nodes that will become orphaned once the node is deleted.
  for (int64_t child_id : ir_graph->dag().DependenciesOf(node_id)) {
    auto child = ir_graph->Get(child_id);
    if (Match(child, Operator())) {
      continue;
    }
    // We shouldn't delete nodes that are used by other nodes.
    if (ir_graph->dag().ParentsOf(child_id).size() > 1) {
      continue;
    }
    // Remove a child if none of its children are Operators.
    bool remove_child = true;
    for (int64_t child_child_id : ir_graph->dag().DependenciesOf(child_id)) {
      auto child_child = ir_graph->Get(child_child_id);
      if (Match(child_child, Operator())) {
        remove_child = false;
        break;
      }
    }
    if (remove_child) {
      nodes_to_remove.push_back(child_id);
    }
  }

  for (auto node_id : nodes_to_remove) {
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(node_id));
  }

  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
