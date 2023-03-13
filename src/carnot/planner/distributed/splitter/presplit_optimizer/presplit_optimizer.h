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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributed/splitter/presplit_optimizer/filter_push_down_rule.h"
#include "src/carnot/planner/distributed/splitter/presplit_optimizer/limit_push_down_rule.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * PreSplitOptimizer performs a PEM and Kelvin-aware optimization on the IR before it is
 * split by the distributed splitter. This phase is intended for optimizations that need to be
 * aware of distributed aspects of the plan, without needing to perform a per-Carnot instance
 * level of optimization that would need to execute on every replicated PEM plan.
 */
class PreSplitOptimizer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<PreSplitOptimizer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<PreSplitOptimizer> optimizer(new PreSplitOptimizer(compiler_state));
    PX_RETURN_IF_ERROR(optimizer->Init());
    return optimizer;
  }

 private:
  explicit PreSplitOptimizer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreateLimitPushdownBatch() {
    // We only run limit pushdown once as it should find all limits that need to be pushed down in a
    // single pass. Otherwise, the Union case will continue pushing Limits up as long as you
    // continue running this rule batch.
    RuleBatch* limit_pushdown = CreateRuleBatch<TryUntilMax>("LimitPushdown", 1);
    limit_pushdown->AddRule<LimitPushdownRule>(compiler_state_);
  }

  void CreateFilterPushdownBatch() {
    // Use TryUntilMax here to avoid swapping the positions of "equal" filters endlessly.
    RuleBatch* filter_pushdown = CreateRuleBatch<TryUntilMax>("FilterPushdown", 1);
    filter_pushdown->AddRule<FilterPushdownRule>(compiler_state_);
  }

  Status Init() {
    CreateLimitPushdownBatch();
    CreateFilterPushdownBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
};
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
