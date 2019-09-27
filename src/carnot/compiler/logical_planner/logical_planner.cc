#include "src/carnot/compiler/logical_planner/logical_planner.h"

#include <utility>

#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {

StatusOr<std::unique_ptr<RelationMap>> LogicalPlanner::MakeRelationMap(const Schema& schema_pb) {
  auto rel_map = std::make_unique<pl::carnot::compiler::RelationMap>();
  for (auto& relation_pair : schema_pb.relation_map()) {
    pl::table_store::schema::Relation rel;
    PL_RETURN_IF_ERROR(rel.FromProto(&relation_pair.second));
    rel_map->emplace(relation_pair.first, rel);
  }

  return rel_map;
}

StatusOr<std::unique_ptr<CompilerState>> LogicalPlanner::CreateCompilerState(
    const distributedpb::LogicalPlannerState& logical_state, RegistryInfo* registry_info) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<compiler::RelationMap> rel_map,
                      MakeRelationMap(logical_state.schema()));

  // Create a CompilerState obj using the relation map and grabbing the current time.

  return std::make_unique<compiler::CompilerState>(std::move(rel_map), registry_info,
                                                   pl::CurrentTimeNS());
}

StatusOr<std::unique_ptr<LogicalPlanner>> LogicalPlanner::Create() {
  auto planner = std::unique_ptr<LogicalPlanner>(new LogicalPlanner());
  PL_RETURN_IF_ERROR(planner->Init());
  // TODO(philkuz) make the pieces
  return planner;
}

Status LogicalPlanner::Init() {
  compiler_ = Compiler();
  // TODO(philkuz) (PL-873) uncomment the following line.
  // PL_ASSIGN_OR_RETURN(distributed_planner_, distributed::DistributedPlanner::Create());
  // TODO(philkuz) (PL-873) remove the following line.
  PL_ASSIGN_OR_RETURN(distributed_planner_, distributed::NoKelvinPlanner::Create());
  return Status::OK();
}

StatusOr<std::unique_ptr<distributed::DistributedPlan>> LogicalPlanner::Plan(
    const distributedpb::LogicalPlannerState& logical_state, const std::string& query) {
  // Compile into the IR.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<RegistryInfo> registry_info, udfexporter::ExportUDFInfo());
  PL_ASSIGN_OR_RETURN(std::unique_ptr<CompilerState> compiler_state,
                      CreateCompilerState(logical_state, registry_info.get()));
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> single_node_plan,
                      compiler_.CompileToIR(query, compiler_state.get()));
  // Create the distributed plan.
  // TODO(philkuz) (PL-873) uncomment this and remove the temporary input.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<distributed::DistributedPlan> distributed_plan,
                      distributed_planner_->Plan(logical_state.distributed_state(),
                                                 compiler_state.get(), single_node_plan.get()));

  // Apply tabletization per elements of the distributed plan.
  PL_RETURN_IF_ERROR(ApplyTabletizer(distributed_plan.get()));
  return distributed_plan;
}

Status LogicalPlanner::ApplyTabletizer(distributed::DistributedPlan* distributed_plan) {
  for (int64_t node_i : distributed_plan->dag().nodes()) {
    auto carnot_instance = distributed_plan->Get(node_i);
    PL_RETURN_IF_ERROR(
        distributed::Tabletizer::Execute(carnot_instance->carnot_info(), carnot_instance->plan()));
  }
  return Status::OK();
}

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
