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

#include <string>

#include "src/carnot/planner/compiler/analyzer/setup_join_type_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;

StatusOr<bool> SetupJoinTypeRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, RightJoin())) {
    PX_RETURN_IF_ERROR(ConvertRightJoinToLeftJoin(static_cast<JoinIR*>(ir_node)));
    return true;
  }
  return false;
}

void SetupJoinTypeRule::FlipColumns(const std::vector<ColumnIR*>& columns) {
  // Update the columns in the output_columns
  for (ColumnIR* col : columns) {
    DCHECK_LT(col->container_op_parent_idx(), 2);
    // 1 -> 0, 0 -> 1
    col->SetContainingOperatorParentIdx(1 - col->container_op_parent_idx());
  }
}

Status SetupJoinTypeRule::ConvertRightJoinToLeftJoin(JoinIR* join_ir) {
  DCHECK_EQ(join_ir->parents().size(), 2UL) << "There should be exactly two parents.";
  DCHECK(join_ir->join_type() == JoinIR::JoinType::kRight);

  std::vector<OperatorIR*> old_parents = join_ir->parents();
  for (OperatorIR* parent : old_parents) {
    PX_RETURN_IF_ERROR(join_ir->RemoveParent(parent));
  }

  PX_RETURN_IF_ERROR(join_ir->AddParent(old_parents[1]));
  PX_RETURN_IF_ERROR(join_ir->AddParent(old_parents[0]));

  FlipColumns(join_ir->left_on_columns());
  FlipColumns(join_ir->right_on_columns());
  FlipColumns(join_ir->output_columns());

  // TODO(philkuz) dependent upon how we actually do anything with output columns, this might change
  if (join_ir->suffix_strs().size() != 0) {
    DCHECK_EQ(join_ir->suffix_strs().size(), 2UL);
    std::string left = join_ir->suffix_strs()[0];
    std::string right = join_ir->suffix_strs()[1];
    join_ir->SetSuffixStrs({right, left});
  }

  return join_ir->SetJoinType(JoinIR::JoinType::kLeft);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
