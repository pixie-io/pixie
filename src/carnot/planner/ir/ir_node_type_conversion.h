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

#include "src/carnot/planner/ir/ir_node_traits.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/types/types.h"

namespace px {
namespace carnot {
namespace planner {

template <typename Node>
struct IRNodeTraits {};

#undef PL_IR_NODE
#define PL_IR_NODE(NAME)                                            \
  template <>                                                       \
  struct IRNodeTraits<NAME##IR> {                                   \
    static constexpr IRNodeType ir_node_type = IRNodeType::k##NAME; \
    static constexpr char name[] = #NAME;                           \
  };
// NOLINTNEXTLINE : build/include
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PL_IR_NODE

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
