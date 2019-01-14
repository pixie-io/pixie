#include <glog/logging.h>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/plan/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace plan {

std::unique_ptr<PlanFragment> PlanFragment::FromProto(const planpb::PlanFragment& pb, int64_t id) {
  auto pf = std::make_unique<PlanFragment>(id);
  auto s = pf->Init(pb);
  // On init failure, return null;
  if (!s.ok()) {
    LOG(ERROR) << "Failed to initialize plan fragment";
    return nullptr;
  }
  return pf;
}
}  // namespace plan
}  // namespace carnot
}  // namespace pl
