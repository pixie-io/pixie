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

#include <vector>

#include "src/carnot/planner/compiler/analyzer/merge_group_by_into_group_acceptor_rule.h"
#include "src/carnot/planner/ir/group_by_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> MergeGroupByIntoGroupAcceptorRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, OperatorWithParent(Operator(), GroupBy())) &&
      ir_node->type() == group_acceptor_type_) {
    return AddGroupByDataIntoGroupAcceptor(static_cast<GroupAcceptorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> MergeGroupByIntoGroupAcceptorRule::AddGroupByDataIntoGroupAcceptor(
    GroupAcceptorIR* acceptor_node) {
  DCHECK_EQ(acceptor_node->parents().size(), 1UL);
  OperatorIR* parent = acceptor_node->parents()[0];
  DCHECK(Match(parent, GroupBy()));
  GroupByIR* groupby = static_cast<GroupByIR*>(parent);
  std::vector<ColumnIR*> new_groups(acceptor_node->groups());
  for (ColumnIR* g : groupby->groups()) {
    PX_ASSIGN_OR_RETURN(ColumnIR * col, CopyColumn(g));
    new_groups.push_back(col);
  }
  PX_RETURN_IF_ERROR(acceptor_node->SetGroups(new_groups));

  DCHECK_EQ(groupby->parents().size(), 1UL);
  OperatorIR* groupby_parent = groupby->parents()[0];

  PX_RETURN_IF_ERROR(acceptor_node->ReplaceParent(groupby, groupby_parent));

  return true;
}

StatusOr<ColumnIR*> MergeGroupByIntoGroupAcceptorRule::CopyColumn(ColumnIR* g) {
  if (Match(g, Metadata())) {
    return g->graph()->CreateNode<MetadataIR>(g->ast(), g->col_name(),
                                              g->container_op_parent_idx());
  }

  return g->graph()->CreateNode<ColumnIR>(g->ast(), g->col_name(), g->container_op_parent_idx());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
