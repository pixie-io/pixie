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

#include <queue>

#include "src/carnot/planner/compiler/analyzer/resolve_stream_rule.h"
#include "src/carnot/planner/ir/stream_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> ResolveStreamRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Stream())) {
    return false;
  }

  auto stream_node = static_cast<StreamIR*>(ir_node);

  // Check for blocking nodes in the ancestors.
  // Currently not supported to stream on queries containing blocking operators.
  DCHECK_EQ(stream_node->parents().size(), 1UL);
  OperatorIR* parent = stream_node->parents()[0];
  std::queue<OperatorIR*> nodes;
  nodes.push(parent);

  while (nodes.size()) {
    auto node = nodes.front();
    nodes.pop();

    if (node->IsBlocking()) {
      return error::Unimplemented("df.stream() not yet supported with the operator $0",
                                  node->DebugString());
    }
    if (Match(node, MemorySource())) {
      static_cast<MemorySourceIR*>(node)->set_streaming(true);
    }
    auto node_parents = node->parents();
    for (OperatorIR* parent : node_parents) {
      nodes.push(parent);
    }
  }

  // The only supported children right now should be MemorySinks.
  auto children = stream_node->Children();
  DCHECK_GT(children.size(), 0U);
  for (OperatorIR* child : children) {
    if (!Match(child, ResultSink())) {
      return error::Unimplemented("df.stream() in the middle of a query is not yet implemented");
    }
    PX_RETURN_IF_ERROR(child->ReplaceParent(stream_node, parent));
  }

  // Now delete the stream node.
  PX_RETURN_IF_ERROR(stream_node->RemoveParent(parent));
  PX_RETURN_IF_ERROR(parent->graph()->DeleteNode(stream_node->id()));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
