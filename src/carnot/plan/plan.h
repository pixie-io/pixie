#pragma once

#include <glog/logging.h>
#include <memory>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {
namespace plan {

class Plan final : public PlanGraph<carnotpb::Plan, PlanFragment, carnotpb::PlanFragment> {};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
