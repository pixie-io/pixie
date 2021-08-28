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

#include "src/carnot/planner/compiler/analyzer/unique_sink_names_rule.h"
#include "src/carnot/planner/ir/memory_sink_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

template <typename TSinkType>
StatusOr<bool> ApplyUniqueSinkName(IRNode* ir_node,
                                   absl::flat_hash_map<std::string, int64_t>* sink_names_count) {
  auto sink = static_cast<TSinkType*>(ir_node);
  bool changed_name = false;
  if (sink_names_count->contains(sink->name())) {
    sink->set_name(absl::Substitute("$0_$1", sink->name(), (*sink_names_count)[sink->name()]++));
    changed_name = true;
  } else {
    (*sink_names_count)[sink->name()] = 1;
  }
  return changed_name;
}

StatusOr<bool> UniqueSinkNameRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, MemorySink())) {
    return ApplyUniqueSinkName<MemorySinkIR>(ir_node, &sink_names_count_);
  }
  if (Match(ir_node, ExternalGRPCSink())) {
    return ApplyUniqueSinkName<GRPCSinkIR>(ir_node, &sink_names_count_);
  }
  return false;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
