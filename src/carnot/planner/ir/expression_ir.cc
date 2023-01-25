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

#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {
StatusOr<absl::flat_hash_set<ColumnIR*>> ExpressionIR::InputColumns() {
  if (Match(this, DataNode())) {
    return absl::flat_hash_set<ColumnIR*>{};
  }
  if (Match(this, ColumnNode())) {
    return absl::flat_hash_set<ColumnIR*>{static_cast<ColumnIR*>(this)};
  }
  if (!Match(this, Func())) {
    return error::Internal("Unexpected Expression type: $0", DebugString());
  }
  absl::flat_hash_set<ColumnIR*> ret;
  auto func = static_cast<FuncIR*>(this);
  for (ExpressionIR* arg : func->args()) {
    PX_ASSIGN_OR_RETURN(auto input_cols, arg->InputColumns());
    ret.insert(input_cols.begin(), input_cols.end());
  }
  return ret;
}

StatusOr<absl::flat_hash_set<std::string>> ExpressionIR::InputColumnNames() {
  PX_ASSIGN_OR_RETURN(auto cols, InputColumns());
  absl::flat_hash_set<std::string> output;
  for (auto col : cols) {
    output.insert(col->col_name());
  }
  return output;
}

Status ExpressionIR::CopyFromNode(const IRNode* node,
                                  absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PX_RETURN_IF_ERROR(IRNode::CopyFromNode(node, copied_nodes_map));
  annotations_ = static_cast<const ExpressionIR*>(node)->annotations();
  type_cast_ = static_cast<const ExpressionIR*>(node)->type_cast();
  return Status::OK();
}

bool ExpressionIR::NodeMatches(IRNode* node) { return Match(node, Expression()); }

}  // namespace planner
}  // namespace carnot
}  // namespace px
