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

class RestrictColumnsRule : public Rule {
  /**
   * @brief Redacts columnar data from MemorySources when the user compiling the
   * query does not have access.
   */
 public:
  explicit RestrictColumnsRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ true, /*reverse_topological_execution*/ false) {
    if (compiler_state->redaction_options().use_full_redaction) {
      redaction_impl_ = std::bind(&RestrictColumnsRule::RedactAllData, this, std::placeholders::_1);
    } else if (compiler_state->redaction_options().use_px_redact_pii_best_effort) {
      redaction_impl_ = std::bind(&RestrictColumnsRule::RedactPIIData, this, std::placeholders::_1);
    }
  }

  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  std::function<StatusOr<OperatorIR*>(MemorySourceIR*)> redaction_impl_;

  StatusOr<OperatorIR*> RedactAllData(MemorySourceIR*);
  StatusOr<OperatorIR*> RedactPIIData(MemorySourceIR*);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
