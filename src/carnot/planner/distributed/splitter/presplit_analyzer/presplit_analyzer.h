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
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/distributed/splitter/presplit_analyzer/split_pem_and_kelvin_only_udf_operator_rule.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * PreSplitAnalyzer performs a PEM and Kelvin-aware transformations on the IR before it is
 * split by the distributed splitter. This phase is intended for transformations that need to be
 * aware of distributed aspects of the plan, without needing to perform a per-Carnot instance
 * level of optimization that would need to execute on every replicated PEM plan. For optimizations,
 * rather than correctness-oriented transformations, see the PreSplitOptimizer.
 */
class PreSplitAnalyzer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<PreSplitAnalyzer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<PreSplitAnalyzer> optimizer(new PreSplitAnalyzer(compiler_state));
    PX_RETURN_IF_ERROR(optimizer->Init());
    return optimizer;
  }

 private:
  explicit PreSplitAnalyzer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreateSplitPEMOnlyUDFsBatch() {
    RuleBatch* split_pem_and_kelvin_udfs = CreateRuleBatch<TryUntilMax>("SplitPEMOnlyUDFs", 1);
    split_pem_and_kelvin_udfs->AddRule<SplitPEMAndKelvinOnlyUDFOperatorRule>(compiler_state_);
  }

  Status Init() {
    CreateSplitPEMOnlyUDFsBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
};
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
