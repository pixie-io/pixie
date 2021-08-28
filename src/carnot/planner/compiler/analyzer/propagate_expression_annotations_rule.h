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

#include <string>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief Propagate the expression annotations to downstream nodes when a column
 * is reassigned or used in another operator. For example, for an integer with annotations that is
 * assigned to be the value of a new column, that new column should have receive the same
 * annotations as the integer that created it.
 *
 */
class PropagateExpressionAnnotationsRule : public Rule {
 public:
  PropagateExpressionAnnotationsRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* node) override;

 private:
  using OperatorOutputAnnotations =
      absl::flat_hash_map<OperatorIR*, absl::flat_hash_map<std::string, ExpressionIR::Annotations>>;
  OperatorOutputAnnotations operator_output_annotations_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
