#include <glog/logging.h>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace plan {

void PlanWalker::CallWalkFn(PlanFragment *pf) { on_plan_fragment_walk_fn_(pf); }

void PlanWalker::Walk(Plan *plan) {
  auto plan_fragments = plan->dag().TopologicalSort();
  for (const auto &node_id : plan_fragments) {
    auto node = plan->nodes().find(node_id);
    if (node == plan->nodes().end()) {
      LOG(WARNING) << absl::StrCat("Could not find node in plan.");
    } else {
      CallWalkFn(node->second.get());
    }
  }
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
