#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/compiler/optimizer/filter_push_down.h"
#include "src/carnot/planner/compiler/optimizer/merge_nodes.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

class Optimizer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<Optimizer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<Optimizer> optimizer(new Optimizer(compiler_state));
    PL_RETURN_IF_ERROR(optimizer->Init());
    return optimizer;
  }

 private:
  explicit Optimizer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreatePruneUnconnectedOpsBatch() {
    RuleBatch* prune_ops_batch = CreateRuleBatch<FailOnMax>("PruneUnconnectedOps", 2);
    prune_ops_batch->AddRule<PruneUnconnectedOperatorsRule>();
  }

  void CreateFilterPushdownBatch() {
    // Use TryUntilMax here to avoid swapping the positions of "equal" filters endlessly.
    RuleBatch* filter_pushdown_batch = CreateRuleBatch<TryUntilMax>("FilterPushdown", 1);
    filter_pushdown_batch->AddRule<FilterPushdownRule>();
  }

  void CreateMergeNodesBatch() {
    RuleBatch* merge_nodes_batch = CreateRuleBatch<TryUntilMax>("MergeNodes", 1);
    merge_nodes_batch->AddRule<MergeNodesRule>(compiler_state_);
  }

  void CreatePruneUnusedColumnsBatch() {
    RuleBatch* prune_unused_columns = CreateRuleBatch<FailOnMax>("PruneUnusedColumns", 2);
    prune_unused_columns->AddRule<PruneUnusedColumnsRule>();
  }

  Status Init() {
    CreatePruneUnconnectedOpsBatch();
    CreateFilterPushdownBatch();
    CreateMergeNodesBatch();
    CreatePruneUnusedColumnsBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
};
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
