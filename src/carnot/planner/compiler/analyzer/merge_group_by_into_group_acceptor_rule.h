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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/group_acceptor_ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule finds every GroupAcceptorIR of the passed in type (i.e. agg or rolling) that
 * follows a groupby and then copies the groups of the groupby into the group acceptor.
 *
 * This rule is not responsible for removing groupbys that satisfy this condition - instead
 * RemoveGroupByRule handles this. There are cases where a groupby might be used by multiple aggs
 * so we can't remove them from the graph.
 *
 */
class MergeGroupByIntoGroupAcceptorRule : public Rule {
 public:
  explicit MergeGroupByIntoGroupAcceptorRule(IRNodeType group_acceptor_type)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        group_acceptor_type_(group_acceptor_type) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> AddGroupByDataIntoGroupAcceptor(GroupAcceptorIR* acceptor_node);
  StatusOr<ColumnIR*> CopyColumn(ColumnIR* g);

  IRNodeType group_acceptor_type_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
