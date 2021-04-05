#include "src/carnot/planner/compiler/optimizer/limit_push_down.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

// Get new locations for the input limit node.
// A single limit may be cloned and pushed up to multiple branches.
absl::flat_hash_set<OperatorIR*> LimitPushdownRule::NewLimitParents(OperatorIR* current_node) {
  // Maps we can simply push up the chain.
  if (Match(current_node, Map())) {
    DCHECK_EQ(1, current_node->parents().size());
    return NewLimitParents(current_node->parents()[0]);
  }
  // Unions will need at most N records from each source.
  if (Match(current_node, Union())) {
    absl::flat_hash_set<OperatorIR*> results;
    // We want 1 Limit node after each union, and one before
    // each of its branches.
    results.insert(current_node);

    for (OperatorIR* parent : current_node->parents()) {
      auto parent_results = NewLimitParents(parent);
      for (OperatorIR* parent_result : parent_results) {
        results.insert(parent_result);
      }
    }
    return results;
  }
  return absl::flat_hash_set<OperatorIR*>{current_node};
}

StatusOr<bool> LimitPushdownRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Limit())) {
    return false;
  }

  auto graph = ir_node->graph();

  LimitIR* limit = static_cast<LimitIR*>(ir_node);
  DCHECK_EQ(1, limit->parents().size());
  OperatorIR* limit_parent = limit->parents()[0];

  auto new_parents = NewLimitParents(limit_parent);
  // If we don't push the limit up at all, just return.
  if (new_parents.size() == 1 && new_parents.find(limit_parent) != new_parents.end()) {
    return false;
  }

  // Remove the limit from its previous location.
  for (OperatorIR* child : limit->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(limit, limit_parent));
  }
  PL_RETURN_IF_ERROR(limit->RemoveParent(limit_parent));

  // Add the limit to its new location(s).
  for (OperatorIR* new_parent : new_parents) {
    PL_ASSIGN_OR_RETURN(LimitIR * new_limit, graph->CopyNode(limit));
    // The parent's children should now be the children of the limit.
    for (OperatorIR* former_child : new_parent->Children()) {
      PL_RETURN_IF_ERROR(former_child->ReplaceParent(new_parent, new_limit));
    }
    // The limit should now be a child of the parent.
    PL_RETURN_IF_ERROR(new_limit->AddParent(new_parent));
    // Ensure we inherit the relation of the parent.
    PL_RETURN_IF_ERROR(new_limit->SetRelation(new_parent->relation()));
  }

  PL_RETURN_IF_ERROR(graph->DeleteNode(limit->id()));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
