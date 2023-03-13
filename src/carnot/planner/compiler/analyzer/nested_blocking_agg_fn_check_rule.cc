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

#include "src/carnot/planner/compiler/analyzer/nested_blocking_agg_fn_check_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> NestedBlockingAggFnCheckRule::Apply(IRNode* ir_node) {
  // Roll the child into the parent so we only have to iterate over the graph once.
  if (!Match(ir_node, BlockingAgg())) {
    return false;
  }
  for (const auto& expr : static_cast<BlockingAggIR*>(ir_node)->aggregate_expressions()) {
    PX_RETURN_IF_ERROR(CheckExpression(expr));
  }
  return false;
}

Status NestedBlockingAggFnCheckRule::CheckExpression(const ColumnExpression& expr) {
  if (!Match(expr.node, Func())) {
    return expr.node->CreateIRNodeError("agg expression must be a function");
  }

  FuncIR* func = static_cast<FuncIR*>(expr.node);
  for (const auto& arg : func->all_args()) {
    if (arg->IsFunction()) {
      return arg->CreateIRNodeError("agg function arg cannot be a function");
    }
  }

  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
