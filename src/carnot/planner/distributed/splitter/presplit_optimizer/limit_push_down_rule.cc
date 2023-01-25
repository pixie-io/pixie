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

#include "src/carnot/planner/distributed/splitter/presplit_optimizer/limit_push_down_rule.h"
#include "src/carnot/planner/distributed/splitter/executor_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

// Get new locations for the input limit node.
// A single limit may be cloned and pushed up to multiple branches.
StatusOr<absl::flat_hash_set<OperatorIR*>> LimitPushdownRule::NewLimitParents(
    OperatorIR* current_node) {
  // Maps we can simply push up the chain.
  if (Match(current_node, Map())) {
    DCHECK_EQ(1U, current_node->parents().size());
    // Don't push a Limit earlier than a PEM-only Map, because we need to ensure that after
    // splitting on Limit nodes, we don't end up with a PEM-only map on the Kelvin side of
    // the distributed plan.
    PX_ASSIGN_OR_RETURN(
        auto has_pem_only_udf,
        HasFuncWithExecutor(compiler_state_, current_node, udfspb::UDFSourceExecutor::UDF_PEM));
    if (!has_pem_only_udf) {
      return NewLimitParents(current_node->parents()[0]);
    }
  }
  // Unions will need at most N records from each source.
  if (Match(current_node, Union())) {
    absl::flat_hash_set<OperatorIR*> results;
    // We want 1 Limit node after each union, and one before
    // each of its branches.
    results.insert(current_node);

    for (OperatorIR* parent : current_node->parents()) {
      PX_ASSIGN_OR_RETURN(auto parent_results, NewLimitParents(parent));
      for (OperatorIR* parent_result : parent_results) {
        results.insert(parent_result);
      }
    }
    return results;
  }
  return absl::flat_hash_set<OperatorIR*>{current_node};
}

StatusOr<bool> LimitPushdownRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Limit())) {
    return false;
  }

  auto graph = ir_node->graph();

  LimitIR* limit = static_cast<LimitIR*>(ir_node);
  DCHECK_EQ(1U, limit->parents().size());
  OperatorIR* limit_parent = limit->parents()[0];

  PX_ASSIGN_OR_RETURN(auto new_parents, NewLimitParents(limit_parent));
  // If we don't push the limit up at all, just return.
  if (new_parents.size() == 1 && new_parents.find(limit_parent) != new_parents.end()) {
    return false;
  }

  // Remove the limit from its previous location.
  for (OperatorIR* child : limit->Children()) {
    PX_RETURN_IF_ERROR(child->ReplaceParent(limit, limit_parent));
  }
  PX_RETURN_IF_ERROR(limit->RemoveParent(limit_parent));

  // Add the limit to its new location(s).
  for (OperatorIR* new_parent : new_parents) {
    PX_ASSIGN_OR_RETURN(LimitIR * new_limit, graph->CopyNode(limit));
    // The parent's children should now be the children of the limit.
    for (OperatorIR* former_child : new_parent->Children()) {
      PX_RETURN_IF_ERROR(former_child->ReplaceParent(new_parent, new_limit));
    }
    // The limit should now be a child of the parent.
    PX_RETURN_IF_ERROR(new_limit->AddParent(new_parent));
    // Ensure we inherit the relation of the parent.
    PX_RETURN_IF_ERROR(new_limit->SetResolvedType(new_parent->resolved_type()));
  }

  PX_RETURN_IF_ERROR(graph->DeleteNode(limit->id()));
  return true;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
