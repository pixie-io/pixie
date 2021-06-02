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

#include "src/carnot/planner/compiler/analyzer/set_memory_source_times_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> SetMemorySourceTimesRule::Apply(IRNode* node) {
  if (!Match(node, MemorySource())) {
    return false;
  }
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(node);
  if (mem_src->IsTimeSet() || !mem_src->HasTimeExpressions()) {
    return false;
  }

  // TODO(philkuz) switch to use TimeIR.
  DCHECK(Match(mem_src->start_time_expr(), Int())) << mem_src->start_time_expr()->DebugString();
  DCHECK(Match(mem_src->end_time_expr(), Int())) << mem_src->end_time_expr()->DebugString();
  mem_src->SetTimeValuesNS(static_cast<IntIR*>(mem_src->start_time_expr())->val(),
                           static_cast<IntIR*>(mem_src->end_time_expr())->val());
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
