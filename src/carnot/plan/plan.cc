#include "src/carnot/plan/plan.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/strings/str_cat.h>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace plan {

Status PlanWalker::CallWalkFn(PlanFragment* pf) { return on_plan_fragment_walk_fn_(pf); }

Status PlanWalker::Walk(Plan* plan) {
  auto plan_fragments = plan->dag().TopologicalSort();
  for (const auto& node_id : plan_fragments) {
    auto node = plan->nodes().find(node_id);
    if (node == plan->nodes().end()) {
      LOG(WARNING) << absl::Substitute("Could not find node $0 in plan", node_id);
    } else {
      PL_RETURN_IF_ERROR(CallWalkFn(node->second.get()));
    }
  }
  return Status::OK();
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
