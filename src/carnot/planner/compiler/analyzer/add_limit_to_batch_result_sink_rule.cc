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

#include "src/carnot/planner/compiler/analyzer/add_limit_to_batch_result_sink_rule.h"
#include "src/carnot/planner/ir/memory_sink_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

bool HasStreamingAncestor(OperatorIR* op) {
  if (Match(op, MemorySource())) {
    return static_cast<MemorySourceIR*>(op)->streaming();
  }
  auto parents = op->parents();
  for (auto parent : parents) {
    if (HasStreamingAncestor(parent)) {
      return true;
    }
  }
  return false;
}

StatusOr<bool> AddLimitToBatchResultSinkRule::Apply(IRNode* ir_node) {
  if (!compiler_state_->has_max_output_rows_per_table()) {
    return false;
  }
  if (!Match(ir_node, ResultSink())) {
    return false;
  }
  if (HasStreamingAncestor(static_cast<OperatorIR*>(ir_node))) {
    return false;
  }

  auto mem_sink = static_cast<MemorySinkIR*>(ir_node);
  DCHECK_EQ(mem_sink->parents().size(), 1UL) << "There should be exactly one parent.";
  auto parent = mem_sink->parents()[0];

  // Update the current limit if it's too small
  if (Match(parent, Limit())) {
    auto limit = static_cast<LimitIR*>(parent);
    DCHECK(limit->limit_value_set());
    if (limit->limit_value() > compiler_state_->max_output_rows_per_table()) {
      limit->SetLimitValue(compiler_state_->max_output_rows_per_table());
      return true;
    }
    return false;
  }

  PX_ASSIGN_OR_RETURN(auto limit,
                      mem_sink->graph()->CreateNode<LimitIR>(
                          mem_sink->ast(), parent, compiler_state_->max_output_rows_per_table()));
  PX_RETURN_IF_ERROR(mem_sink->ReplaceParent(parent, limit));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
