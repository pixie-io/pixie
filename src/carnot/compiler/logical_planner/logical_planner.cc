#include "src/carnot/compiler/logical_planner/logical_planner.h"
#include "src/carnot/compiler/distributed_analyzer.h"

#include <utility>

#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {

using table_store::schemapb::Schema;

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

StatusOr<std::unique_ptr<LogicalPlanner>> LogicalPlanner::Create(const udfspb::UDFInfo& udf_info) {
  auto planner = std::unique_ptr<LogicalPlanner>(new LogicalPlanner());
  PL_RETURN_IF_ERROR(planner->Init(udf_info));
  return planner;
}

Status LogicalPlanner::Init(const udfspb::UDFInfo& udf_info) {
  compiler_ = Compiler();
  registry_info_ = std::make_unique<compiler::RegistryInfo>();
  PL_RETURN_IF_ERROR(registry_info_->Init(udf_info));

  PL_ASSIGN_OR_RETURN(distributed_planner_, distributed::DistributedPlanner::Create());
  return Status::OK();
}

StatusOr<std::unique_ptr<distributed::DistributedPlan>> LogicalPlanner::Plan(
    const distributedpb::LogicalPlannerState& logical_state,
    const plannerpb::QueryRequest& query_request) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<RegistryInfo> registry_info, udfexporter::ExportUDFInfo());
  // Compile into the IR.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<CompilerState> compiler_state,
                      CreateCompilerState(logical_state, registry_info.get()));
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> single_node_plan,
                      compiler_.CompileToIR(query_request.query_str(), compiler_state.get()));
  // Create the distributed plan.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<distributed::DistributedPlan> distributed_plan,
                      distributed_planner_->Plan(logical_state.distributed_state(),
                                                 compiler_state.get(), single_node_plan.get()));

  PL_ASSIGN_OR_RETURN(std::unique_ptr<distributed::DistributedAnalyzer> analyzer,
                      distributed::DistributedAnalyzer::Create());
  PL_RETURN_IF_ERROR(analyzer->Execute(distributed_plan.get()));
  return distributed_plan;
}

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
