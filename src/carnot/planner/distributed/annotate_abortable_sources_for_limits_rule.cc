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

#include <utility>

#include "src/carnot/planner/distributed/annotate_abortable_sources_for_limits_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<bool> AnnotateAbortableSourcesForLimitsRule::Apply(IRNode* node) {
  if (!Match(node, Limit())) {
    return false;
  }
  IR* graph = node->graph();
  auto limit = static_cast<LimitIR*>(node);

  plan::DAG dag_copy = graph->dag();
  dag_copy.DeleteNode(limit->id());
  auto src_nodes = graph->FindNodesThatMatch(Source());
  auto sink_nodes = graph->FindNodesThatMatch(Sink());
  bool changed = false;
  for (const auto& src : src_nodes) {
    auto transitive_deps = dag_copy.TransitiveDepsFrom(src->id());
    bool is_abortable = true;
    for (const auto sink : sink_nodes) {
      if (transitive_deps.find(sink->id()) != transitive_deps.end()) {
        // If there is a sink in the transitive children of the source node then this source node
        // is not abortable from this limit node.
        is_abortable = false;
        break;
      }
    }
    if (is_abortable) {
      limit->AddAbortableSource(src->id());
      changed = true;
    }
  }
  return changed;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
