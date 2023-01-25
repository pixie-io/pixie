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

#include "src/carnot/plan/plan.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/strings/str_cat.h>

#include "src/carnot/dag/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace plan {

Status PlanWalker::CallWalkFn(PlanFragment* pf) { return on_plan_fragment_walk_fn_(pf); }

Status PlanWalker::Walk(Plan* plan) {
  auto plan_fragments = plan->dag().TopologicalSort();
  for (const auto& node_id : plan_fragments) {
    auto node = plan->nodes().find(node_id);
    if (node == plan->nodes().end()) {
      LOG(WARNING) << absl::Substitute("Could not find node $0 in plan", node_id);
    } else {
      PX_RETURN_IF_ERROR(CallWalkFn(node->second.get()));
    }
  }
  return Status::OK();
}

}  // namespace plan
}  // namespace carnot
}  // namespace px
