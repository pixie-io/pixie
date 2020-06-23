#include "src/carnot/planner/distributed/distributed_plan.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<distributedpb::DistributedPlan> DistributedPlan::ToProto() const {
  distributedpb::DistributedPlan physical_plan_pb;
  auto physical_plan_dag = physical_plan_pb.mutable_dag();
  auto qb_address_to_plan_pb = physical_plan_pb.mutable_qb_address_to_plan();
  auto qb_address_to_dag_id_pb = physical_plan_pb.mutable_qb_address_to_dag_id();

  for (int64_t i : dag_.TopologicalSort()) {
    CarnotInstance* carnot = Get(i);
    CHECK_EQ(carnot->id(), i) << absl::Substitute("Index in node ($1) and DAG ($0) don't agree.", i,
                                                  carnot->id());
    DCHECK(carnot->plan()) << absl::Substitute("$0 doesn't have a plan set.",
                                               carnot->DebugString());
    PL_ASSIGN_OR_RETURN(auto plan_proto, carnot->PlanProto());
    for (int64_t parent_i : dag_.ParentsOf(i)) {
      *(plan_proto.add_incoming_agent_ids()) = Get(parent_i)->carnot_info().agent_id();
    }
    (*qb_address_to_plan_pb)[carnot->QueryBrokerAddress()] = plan_proto;
    (*qb_address_to_dag_id_pb)[carnot->QueryBrokerAddress()] = i;

    auto plan_opts = (*qb_address_to_plan_pb)[carnot->QueryBrokerAddress()].mutable_plan_options();
    plan_opts->CopyFrom(plan_options_);
  }
  dag_.ToProto(physical_plan_dag);
  return physical_plan_pb;
}

StatusOr<int64_t> DistributedPlan::AddCarnot(const distributedpb::CarnotInfo& carnot_info) {
  int64_t carnot_id = id_counter_;
  ++id_counter_;
  PL_ASSIGN_OR_RETURN(auto instance, CarnotInstance::Create(carnot_id, carnot_info, this));
  id_to_node_map_.emplace(carnot_id, std::move(instance));
  dag_.AddNode(carnot_id);
  return carnot_id;
}

StatusOr<std::unique_ptr<CarnotInstance>> CarnotInstance::Create(
    int64_t id, const distributedpb::CarnotInfo& carnot_info, DistributedPlan* parent_plan) {
  if (carnot_info.has_metadata_info()) {
    PL_ASSIGN_OR_RETURN(auto bf, md::AgentMetadataFilter::FromProto(carnot_info.metadata_info()));
    return std::unique_ptr<CarnotInstance>(
        new CarnotInstance(id, carnot_info, parent_plan, std::move(bf)));
  }
  return std::unique_ptr<CarnotInstance>(new CarnotInstance(id, carnot_info, parent_plan, nullptr));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
