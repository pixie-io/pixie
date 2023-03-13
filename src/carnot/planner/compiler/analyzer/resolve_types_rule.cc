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

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> ResolveTypesRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Operator())) {
    return false;
  }
  auto op = static_cast<OperatorIR*>(ir_node);
  if (op->is_type_resolved()) {
    return false;
  }
  PX_RETURN_IF_ERROR(ResolveOperatorType(op, compiler_state_));

  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
