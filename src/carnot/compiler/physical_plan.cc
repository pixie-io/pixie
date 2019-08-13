#include "src/carnot/compiler/physical_plan.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {
compilerpb::PhysicalPlan PhysicalPlan::ToProto() const {
  compilerpb::PhysicalPlan physical_plan_pb;
  auto physical_plan_dag = physical_plan_pb.mutable_dag();
  auto qb_address_to_plan_pb = physical_plan_pb.mutable_qb_address_to_plan();
  auto qb_address_to_dag_id_pb = physical_plan_pb.mutable_qb_address_to_dag_id();

  // TODO(philkuz) Move compiler IRToLogicalPlan into IR::ToProto to make this usable.
  PL_UNUSED(qb_address_to_plan_pb);

  for (int64_t i : dag_.TopologicalSort()) {
    auto id_node_map_iter = id_to_node_map_.find(i);
    CHECK(id_node_map_iter != id_to_node_map_.end()) << "Couldn't find index: " << i;
    const CarnotInstance& carnot = id_node_map_iter->second;
    CHECK_EQ(carnot.id(), i) << absl::Substitute("Index in node ($1) and DAG ($0) don't agree.", i,
                                                 carnot.id());
    // (*qb_address_to_plan_pb)[carnot.QueryBrokerAddress()] = carnot->PlanProto();
    (*qb_address_to_dag_id_pb)[carnot.QueryBrokerAddress()] = i;

    // Handle the dag side.
    auto dag_node = physical_plan_dag->add_nodes();
    dag_node->set_id(i);
    for (const auto& dep : dag_.DependenciesOf(i)) {
      dag_node->add_sorted_deps(dep);
    }
  }
  return physical_plan_pb;
}
}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
