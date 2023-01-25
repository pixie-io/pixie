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
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/ir/time.h"
#include "src/carnot/planner/metadata/metadata_handler.h"

namespace px {
namespace carnot {
namespace planner {

template <typename TPlan>
struct RuleTraits {};

template <>
struct RuleTraits<IR> {
  using node_type = IRNode;
};

template <typename TPlan>
class BaseRule {
 public:
  BaseRule() = delete;
  BaseRule(CompilerState* compiler_state, bool use_topo, bool reverse_topological_execution)
      : compiler_state_(compiler_state),
        use_topo_(use_topo),
        reverse_topological_execution_(reverse_topological_execution) {}

  virtual ~BaseRule() = default;

  virtual StatusOr<bool> Execute(TPlan* graph) {
    bool any_changed = false;
    if (!use_topo_) {
      PX_ASSIGN_OR_RETURN(any_changed, ExecuteUnsorted(graph));
    } else {
      PX_ASSIGN_OR_RETURN(any_changed, ExecuteTopologicalSorted(graph));
    }
    PX_RETURN_IF_ERROR(EmptyDeleteQueue(graph));
    return any_changed;
  }

 protected:
  StatusOr<bool> ExecuteTopologicalSorted(TPlan* graph) {
    bool any_changed = false;
    std::vector<int64_t> topo_graph = graph->dag().TopologicalSort();
    if (reverse_topological_execution_) {
      std::reverse(topo_graph.begin(), topo_graph.end());
    }
    for (int64_t node_i : topo_graph) {
      // The node may have been deleted by a prior call to Apply on a parent or child node.
      if (!graph->HasNode(node_i)) {
        continue;
      }
      PX_ASSIGN_OR_RETURN(bool node_is_changed, Apply(graph->Get(node_i)));
      any_changed = any_changed || node_is_changed;
    }
    return any_changed;
  }

  StatusOr<bool> ExecuteUnsorted(TPlan* graph) {
    bool any_changed = false;
    // We need to copy over nodes because the Apply() might add nodes which can affect traversal,
    // causing nodes to be skipped.
    auto nodes = graph->dag().nodes();
    for (int64_t node_i : nodes) {
      // The node may have been deleted by a prior call to Apply on a parent or child node.
      if (!graph->HasNode(node_i)) {
        continue;
      }
      PX_ASSIGN_OR_RETURN(bool node_is_changed, Apply(graph->Get(node_i)));
      any_changed = any_changed || node_is_changed;
    }
    return any_changed;
  }

  /**
   * @brief Applies the rule to a node.
   * Should include a check for type and should return true if it changes the node.
   * It can pass potential compiler errors through the Status.
   *
   * @param node - the node to apply this rule to.
   * @return true: if the rule changes the node.
   * @return false: if the rule does nothing to the node.
   * @return Status: error if something goes wrong during the rule application.
   */
  virtual StatusOr<bool> Apply(typename RuleTraits<TPlan>::node_type* node) = 0;
  void DeferNodeDeletion(int64_t node) { node_delete_q.push(node); }

  Status EmptyDeleteQueue(TPlan* graph) {
    while (!node_delete_q.empty()) {
      PX_RETURN_IF_ERROR(graph->DeleteNode(node_delete_q.front()));
      node_delete_q.pop();
    }
    return Status::OK();
  }

  // The queue containing nodes to delete.
  std::queue<int64_t> node_delete_q;
  CompilerState* compiler_state_;
  bool use_topo_;
  bool reverse_topological_execution_;
};

using Rule = BaseRule<IR>;

}  // namespace planner
}  // namespace carnot
}  // namespace px
