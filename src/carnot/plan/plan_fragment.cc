#include <glog/logging.h>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/plan/proto/plan.pb.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace plan {

using pl::Status;

Status PlanFragment::Init(const planpb::PlanFragment& pb) {
  // Add all of the nodes into the DAG.
  for (const auto& node : pb.dag().nodes()) {
    dag_.AddNode(node.id());
  }

  // Add all of the edges into the DAG.
  for (const auto& node : pb.dag().nodes()) {
    for (int64_t to_node : node.sorted_deps()) {
      dag_.AddEdge(node.id(), to_node);
    }
  }

  for (const auto& node : pb.nodes()) {
    nodes_.emplace(Operator::FromProto(node.op(), node.id()));
  }

  is_initialized_ = true;
  return Status::OK();
}
}  // namespace plan
}  // namespace carnot
}  // namespace pl
