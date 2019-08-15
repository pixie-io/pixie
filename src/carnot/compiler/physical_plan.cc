#include "src/carnot/compiler/physical_plan.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {
StatusOr<compilerpb::PhysicalPlan> PhysicalPlan::ToProto() const {
  compilerpb::PhysicalPlan physical_plan_pb;
  auto physical_plan_dag = physical_plan_pb.mutable_dag();
  auto qb_address_to_plan_pb = physical_plan_pb.mutable_qb_address_to_plan();
  auto qb_address_to_dag_id_pb = physical_plan_pb.mutable_qb_address_to_dag_id();

  for (int64_t i : dag_.TopologicalSort()) {
    CarnotInstance* carnot = Get(i);
    CHECK_EQ(carnot->id(), i) << absl::Substitute("Index in node ($1) and DAG ($0) don't agree.", i,
                                                  carnot->id());
    DCHECK(carnot->plan()) << absl::Substitute("$0 doesn't have a plan set.",
                                               carnot->DebugString());
    PL_ASSIGN_OR_RETURN((*qb_address_to_plan_pb)[carnot->QueryBrokerAddress()],
                        carnot->PlanProto());
    (*qb_address_to_dag_id_pb)[carnot->QueryBrokerAddress()] = i;

    // Handle the dag side.
    auto dag_node = physical_plan_dag->add_nodes();
    dag_node->set_id(i);
    for (const auto& dep : dag_.DependenciesOf(i)) {
      dag_node->add_sorted_deps(dep);
    }
  }
  return physical_plan_pb;
}

int64_t PhysicalPlan::AddCarnot(const compilerpb::CarnotInfo& carnot_info) {
  int64_t carnot_id = id_counter_;
  ++id_counter_;
  auto instance = std::make_unique<CarnotInstance>(carnot_id, carnot_info);
  id_to_node_map_.emplace(carnot_id, std::move(instance));
  dag_.AddNode(carnot_id);
  return carnot_id;
}
}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
