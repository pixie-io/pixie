#pragma once

#include <glog/logging.h>
#include <memory>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/plan/proto/plan.pb.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace plan {

class Plan final : public PlanGraph<planpb::Plan, PlanFragment, planpb::PlanFragment> {};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
