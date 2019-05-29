#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

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

template <typename T, typename TWalkFunc>
void PlanFragmentWalker::CallAs(const TWalkFunc& fn, const Operator& op) {
  if (!fn) {
    VLOG(google::WARNING) << "fn does not exist";
  }
  return fn(static_cast<const T&>(op));
}

void PlanFragmentWalker::CallWalkFn(const Operator& op) {
  const auto op_type = op.op_type();
  switch (op_type) {
    case planpb::OperatorType::MEMORY_SOURCE_OPERATOR:
      CallAs<MemorySourceOperator>(on_memory_source_walk_fn_, op);
      break;
    case planpb::OperatorType::MAP_OPERATOR:
      CallAs<MapOperator>(on_map_walk_fn_, op);
      break;
    case planpb::OperatorType::BLOCKING_AGGREGATE_OPERATOR:
      CallAs<BlockingAggregateOperator>(on_blocking_aggregate_walk_fn_, op);
      break;
    case planpb::OperatorType::MEMORY_SINK_OPERATOR:
      CallAs<MemorySinkOperator>(on_memory_sink_walk_fn_, op);
      break;
    case planpb::OperatorType::FILTER_OPERATOR:
      CallAs<FilterOperator>(on_filter_walk_fn_, op);
      break;
    case planpb::OperatorType::LIMIT_OPERATOR:
      CallAs<LimitOperator>(on_limit_walk_fn_, op);
      break;
    default:
      LOG(WARNING) << absl::StrCat("Operator does not exist.");
  }
}

void PlanFragmentWalker::Walk(PlanFragment* plan_fragment) {
  auto operators = plan_fragment->dag().TopologicalSort();
  for (const auto& node_id : operators) {
    auto node = plan_fragment->nodes().find(node_id);
    if (node == plan_fragment->nodes().end()) {
      LOG(WARNING) << absl::StrCat("Could not find node in plan fragment");
    } else {
      CallWalkFn(*node->second);
    }
  }
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
