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

#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief This rule pushes limits as early in the IR as possible, without pushing them
 * past PEM-only operators.
 */
class LimitPushdownRule : public Rule {
 public:
  explicit LimitPushdownRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode*) override;

 private:
  StatusOr<absl::flat_hash_set<OperatorIR*>> NewLimitParents(OperatorIR* current_node);
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
