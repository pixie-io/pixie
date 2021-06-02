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
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule makes sure that nested blocking aggs cause a compiler error.
 *
 */
class NestedBlockingAggFnCheckRule : public Rule {
 public:
  NestedBlockingAggFnCheckRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  Status CheckExpression(const ColumnExpression& col_expr);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
