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

#include <absl/container/flat_hash_map.h>
#include <sole.hpp>
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/rules/rule_executor.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
template <>
struct RuleTraits<distributed::DistributedPlan> {
  using node_type = distributed::CarnotInstance;
};

namespace distributed {
using DistributedRule = BaseRule<distributed::DistributedPlan>;
using DistributedRuleBatch = BaseRuleBatch<DistributedRule>;

/**
 * @brief This class supports running an IR graph rule (independently) over each IR graph of a
 * DistributedPlan. This is distinct from other DistributedRules, which may modify the
 * CarnotInstances and DistributedPlan dag.
 * Note that this rule shares the state of its inner rule across all Carnot instances.
 *
 */
template <typename TRule>
class DistributedIRRule : public DistributedRule {
 public:
  DistributedIRRule()
      : DistributedRule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {
    subrule_ = std::make_unique<TRule>();
  }

  // Used for testing.
  TRule* subrule() { return subrule_.get(); }

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override {
    return subrule_->Execute(node->plan());
  }

  std::unique_ptr<TRule> subrule_;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
