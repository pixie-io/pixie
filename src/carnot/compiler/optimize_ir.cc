#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/optimize_ir.h"
#include "src/common/time.h"

namespace pl {
namespace carnot {
namespace compiler {

Status IROptimizer::Optimize(IR* ir_graph) {
  PL_RETURN_IF_ERROR(CollapseRange(ir_graph));
  return Status::OK();
}

Status IROptimizer::CollapseRange(IR* ir_graph) {
  auto dag = ir_graph->dag();
  auto sorted_dag = dag.TopologicalSort();

  // This assumes there is only one Range in the query.
  RangeIR* rangeIR = nullptr;
  MemorySourceIR* srcIR;
  StringIR* timeIR;
  for (const auto& node_id : sorted_dag) {
    auto node = ir_graph->Get(node_id);
    if (node->type() != IRNodeType::RangeType) {
      continue;
    }
    rangeIR = static_cast<RangeIR*>(node);
    // Already preverified that rangeIR is child of MemSourceIR.
    srcIR = static_cast<MemorySourceIR*>(rangeIR->parent());
    ir_graph->DeleteEdge(srcIR->id(), rangeIR->id());

    timeIR = static_cast<StringIR*>(rangeIR->time_repr());
    // TODO(philkuz) merge with Zain's clock now time.
    auto now = std::chrono::high_resolution_clock::now();

    auto now_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    auto time_str = timeIR->str();
    auto s = StringToTimeInt(time_str);
    // TODO(michelle) (PL-406): Update when we add kwargs to the compiler.
    if (!s.ok()) {
      // Time string is in format "1,20".
      PL_ASSIGN_OR_RETURN(auto range, StringToTimeRange(time_str));
      srcIR->SetTime(range.first, range.second);
    } else {
      // Time string is in format "-2m".
      auto time_difference = s.ConsumeValueOrDie();
      srcIR->SetTime(now_ns + time_difference, now_ns);
    }
    ir_graph->DeleteNode(timeIR->id());

    // Update all of range's dependencies to point to src.
    for (const auto& dep_id : ir_graph->dag().DependenciesOf(rangeIR->id())) {
      auto dep = ir_graph->Get(dep_id);
      ir_graph->DeleteEdge(rangeIR->id(), dep_id);
      PL_RETURN_IF_ERROR(ir_graph->AddEdge(srcIR->id(), dep_id));
      if (dep->IsOp()) {
        auto casted_node = static_cast<OperatorIR*>(dep);
        PL_RETURN_IF_ERROR(casted_node->SetParent(dynamic_cast<IRNode*>(srcIR)));
      }
    }
    break;
  }
  if (rangeIR != nullptr) {
    ir_graph->DeleteNode(rangeIR->id());
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
