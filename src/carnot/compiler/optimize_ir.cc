#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/optimize_ir.h"
#include "src/common/base/base.h"

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
  RangeIR* range_ir = nullptr;
  MemorySourceIR* src_ir;
  IntIR* start_time_ir;
  IntIR* stop_time_ir;
  for (const auto& node_id : sorted_dag) {
    auto node = ir_graph->Get(node_id);
    if (node->type() != IRNodeType::kRange) {
      continue;
    }
    range_ir = static_cast<RangeIR*>(node);

    DCHECK_EQ(range_ir->parents().size(), 1UL);
    OperatorIR* range_parent = range_ir->parents()[0];

    DCHECK(range_parent->type() == IRNodeType::kMemorySource)
        << "Expected range parent to be a MemorySource, not a " << range_parent->type_string();
    src_ir = static_cast<MemorySourceIR*>(range_parent);
    PL_RETURN_IF_ERROR(ir_graph->DeleteEdge(src_ir->id(), range_ir->id()));

    start_time_ir = static_cast<IntIR*>(range_ir->start_repr());
    stop_time_ir = static_cast<IntIR*>(range_ir->stop_repr());

    src_ir->SetTime(start_time_ir->val(), stop_time_ir->val());

    PL_RETURN_IF_ERROR(ir_graph->DeleteNode(start_time_ir->id()));
    PL_RETURN_IF_ERROR(ir_graph->DeleteNode(stop_time_ir->id()));

    // Update all of range's dependencies to point to src.
    for (const auto& dep_id : ir_graph->dag().DependenciesOf(range_ir->id())) {
      auto dep = ir_graph->Get(dep_id);
      if (!dep->IsOp()) {
        PL_RETURN_IF_ERROR(ir_graph->DeleteEdge(range_ir->id(), dep_id));
        PL_RETURN_IF_ERROR(ir_graph->AddEdge(src_ir->id(), dep_id));
        continue;
      }
      auto casted_node = static_cast<OperatorIR*>(dep);
      PL_RETURN_IF_ERROR(casted_node->RemoveParent(range_ir));
      PL_RETURN_IF_ERROR(casted_node->AddParent(src_ir));
    }
    break;
  }
  if (range_ir != nullptr) {
    PL_RETURN_IF_ERROR(ir_graph->DeleteNode(range_ir->id()));
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
