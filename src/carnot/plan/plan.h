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
