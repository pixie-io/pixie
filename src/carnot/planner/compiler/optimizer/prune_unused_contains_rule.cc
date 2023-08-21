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

#include "src/carnot/planner/compiler/optimizer/prune_unused_contains_rule.h"

#include <algorithm>
#include <queue>

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> PruneUnusedContainsRule::RemoveMatchingFilter(IRNode* ir_node) {
  auto ir_graph = ir_node->graph();
  auto node_id = ir_node->id();
  if (!Match(ir_node, Filter())) return false;

  FilterIR* filter = static_cast<FilterIR*>(ir_node);
  ExpressionIR* expr = filter->filter_expr();

  if (!Match(expr, Func("contains"))) return false;

  FuncIR* func = static_cast<FuncIR*>(expr);
  auto args = func->all_args();
  auto str = args[0];
  auto substr = args[1];

  if (!Match(substr, String(""))) {
    return false;
  }

  DCHECK_EQ(ir_graph->dag().ParentsOf(node_id).size(), 1UL);
  auto parent_id = ir_graph->dag().ParentsOf(node_id)[0];
  auto parent_node = ir_graph->Get(parent_id);

  PX_RETURN_IF_ERROR(ir_graph->DeleteNode(func->id()));

  // str and substr may be connected to other nodes in the
  // IR graph. We should only delete those nodes once their
  // last parent is removed. This should happen with the
  // preceding FuncIR deletion.
  if (ir_graph->dag().ParentsOf(str->id()).size() == 0) {
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(str->id()));
  }
  if (ir_graph->dag().ParentsOf(substr->id()).size() == 0) {
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(substr->id()));
  }

  // Reparent any child nodes of the filter
  for (int64_t child_id : ir_graph->dag().DependenciesOf(node_id)) {
    auto child_node = ir_graph->Get(child_id);

    if (!Match(child_node, Operator())) {
      continue;
    }
    auto child_op_node = static_cast<OperatorIR*>(child_node);

    PX_RETURN_IF_ERROR(child_op_node->ReplaceParent(static_cast<OperatorIR*>(ir_node),
                                                    static_cast<OperatorIR*>(parent_node)));
  }
  PX_RETURN_IF_ERROR(ir_graph->DeleteNode(node_id));
  return true;
}

StatusOr<bool> PruneUnusedContainsRule::Apply(IRNode* ir_node) {
  return RemoveMatchingFilter(ir_node);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
