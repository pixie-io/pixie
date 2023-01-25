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
#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/ir/all_ir_nodes.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/ir_node_traits.h"
#include "src/carnot/planner/types/types.h"

namespace px {
namespace carnot {
namespace planner {

template <typename Node>
struct IRNodeTraits {};

#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME)                                     \
  template <>                                                       \
  struct IRNodeTraits<NAME##IR> {                                   \
    static constexpr IRNodeType ir_node_type = IRNodeType::k##NAME; \
    static constexpr char name[] = #NAME;                           \
  };
// NOLINTNEXTLINE : build/include
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PX_CARNOT_IR_NODE

template <>
struct IRNodeTraits<ExpressionIR> {
  static constexpr IRNodeType ir_node_type = IRNodeType::kAny;
  static constexpr char name[] = "expression";
};

template <>
struct IRNodeTraits<OperatorIR> {
  static constexpr IRNodeType ir_node_type = IRNodeType::kAny;
  static constexpr char name[] = "operator";
};

template <>
struct IRNodeTraits<IRNode> {
  static constexpr IRNodeType ir_node_type = IRNodeType::kAny;
  static constexpr char name[] = "general";
};

template <typename TIRNode>
inline StatusOr<TIRNode*> AsNodeType(IRNode* node, std::string_view node_name) {
  if (!TIRNode::NodeMatches(node)) {
    return node->CreateIRNodeError("Expected arg '$0' as type '$1', received '$2'", node_name,
                                   IRNodeTraits<TIRNode>::name, node->type_string());
  }
  return static_cast<TIRNode*>(node);
}

template <>
inline StatusOr<IRNode*> AsNodeType<IRNode>(IRNode* node, std::string_view /* node_name */) {
  return node;
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
