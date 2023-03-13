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

#include "src/carnot/planner/compiler/optimizer/prune_unused_columns_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> PruneUnusedColumnsRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Operator())) {
    return false;
  }
  auto op = static_cast<OperatorIR*>(ir_node);
  DCHECK(op->is_type_resolved());
  auto changed = false;

  if (operator_to_required_outputs_.contains(op)) {
    auto required_outs = operator_to_required_outputs_.at(op);
    auto prev_type = op->resolved_table_type();
    PX_RETURN_IF_ERROR(op->PruneOutputColumnsTo(required_outs));
    auto new_type = op->resolved_table_type();
    changed = !prev_type->Equals(new_type);
  }

  PX_ASSIGN_OR_RETURN(auto required_inputs, op->RequiredInputColumns());
  for (const auto& [parent_idx, required_columns] : Enumerate(required_inputs)) {
    auto parent_ptr = op->parents()[parent_idx];
    operator_to_required_outputs_[parent_ptr].insert(required_columns.begin(),
                                                     required_columns.end());
  }

  return changed;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
