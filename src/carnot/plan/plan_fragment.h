#pragma once

#include <glog/logging.h>
#include <memory>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/plan/proto/plan.pb.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace plan {

class PlanFragment final : public PlanGraph<planpb::PlanFragment, std::unique_ptr<Operator>> {
 public:
  Status Init(const planpb::PlanFragment &pb);
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
