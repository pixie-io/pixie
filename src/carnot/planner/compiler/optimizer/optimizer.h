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
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/compiler/optimizer/merge_nodes_rule.h"
#include "src/carnot/planner/compiler/optimizer/prune_unconnected_operators_rule.h"
#include "src/carnot/planner/compiler/optimizer/prune_unused_columns_rule.h"
#include "src/carnot/planner/compiler/optimizer/prune_unused_contains_rule.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class Optimizer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<Optimizer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<Optimizer> optimizer(new Optimizer(compiler_state));
    PX_RETURN_IF_ERROR(optimizer->Init());
    return optimizer;
  }

 private:
  explicit Optimizer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreatePruneUnconnectedOpsBatch() {
    RuleBatch* prune_ops_batch = CreateRuleBatch<FailOnMax>("PruneUnconnectedOps", 2);
    prune_ops_batch->AddRule<PruneUnconnectedOperatorsRule>();
  }

  void CreateMergeNodesBatch() {
    RuleBatch* merge_nodes_batch = CreateRuleBatch<TryUntilMax>("MergeNodes", 1);
    merge_nodes_batch->AddRule<MergeNodesRule>(compiler_state_);
  }

  void CreatePruneUnusedColumnsBatch() {
    RuleBatch* prune_unused_columns = CreateRuleBatch<FailOnMax>("PruneUnusedColumns", 2);
    prune_unused_columns->AddRule<PruneUnusedColumnsRule>();
  }

  void CreatePruneUnusedContainsBatch() {
    RuleBatch* prune_unused_columns = CreateRuleBatch<DoOnce>("PruneUnusedContains");
    prune_unused_columns->AddRule<PruneUnusedContainsRule>();
  }

  Status Init() {
    CreatePruneUnconnectedOpsBatch();
    CreateMergeNodesBatch();
    CreatePruneUnusedColumnsBatch();
    CreatePruneUnusedContainsBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
};
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
