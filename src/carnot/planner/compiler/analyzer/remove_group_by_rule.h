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

#include "src/carnot/planner/ir/group_by_ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule removes groupbys after the aggregate merging step.
 *
 * We can't remove the groupby in the aggregate merging step because it's possible
 * that mutliple aggregates branch off of a single groupby. We need to do that here.
 *
 * This rule succeeds if it finds that GroupBy has no more children. If it does, this
 * rule will throw an error.
 *
 */
class RemoveGroupByRule : public Rule {
 public:
  RemoveGroupByRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> RemoveGroupBy(GroupByIR* ir_node);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
