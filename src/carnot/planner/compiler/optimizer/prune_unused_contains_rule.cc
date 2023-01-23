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

StatusOr<bool> PruneUnusedContainsRule::Apply(IRNode* ir_node) {
  auto ir_graph = ir_node->graph();
  auto node_id = ir_node->id();
  if (!Match(ir_node, Filter())) return false;

  FilterIR* filter = static_cast<FilterIR*>(ir_node);
  ExpressionIR* expr = filter->filter_expr();

  if (!Match(expr, Func("contains"))) return false;

  FuncIR* func = static_cast<FuncIR*>(expr);
  auto args = func->all_args();
  auto contains_substr = args[1];

  if (Match(contains_substr, String(""))) {
    // Delete the filter, contains function and its arguments
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(node_id));
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(func->id()));
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(args[0]->id()));
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(args[1]->id()));

    return true;
  }
  return false;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
