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

#include <functional>
#include <memory>

#include "src/carnot/dag/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace plan {

class Plan final : public PlanGraph<planpb::Plan, PlanFragment, planpb::PlanFragment> {};

/**
 * A walker that walks the plan fragments of a plan in a topologically-sorted order.
 * A sample usage is:
 *   PlanWalker()
 *      .OnPlanFragment([&](auto& pf) {
 *     })
// *     .Walk(&plan);
 */
class PlanWalker {
 public:
  using PlanFragmentWalkFn = std::function<Status(PlanFragment*)>;

  /**
   * Register callback for each plan fragment.
   * @param fn The function to call for each plan fragment.
   * @return self to allow chaining
   */
  PlanWalker& OnPlanFragment(const PlanFragmentWalkFn& fn) {
    on_plan_fragment_walk_fn_ = fn;
    return *this;
  }

  /**
   * Perform a walk of the plan fragments in a topologically-sorted order.
   * @param plan The plan to walk.
   */
  Status Walk(Plan* plan);

 private:
  Status CallWalkFn(PlanFragment* pf);

  PlanFragmentWalkFn on_plan_fragment_walk_fn_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace px
