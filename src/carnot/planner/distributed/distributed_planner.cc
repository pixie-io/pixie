#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/distributed/distributed_analyzer.h"
#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/rules/rules.h"
namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<std::unique_ptr<DistributedPlanner>> DistributedPlanner::Create() {
  std::unique_ptr<DistributedPlanner> planner(new DistributedPlanner());
  PL_RETURN_IF_ERROR(planner->Init());
  return planner;
}

Status DistributedPlanner::Init() { return Status::OK(); }

StatusOr<std::unique_ptr<DistributedPlan>> DistributedPlanner::Plan(
    const distributedpb::DistributedState& distributed_state, CompilerState*,
    const IR* logical_plan) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Coordinator> coordinator,
                      Coordinator::Create(distributed_state));

  PL_ASSIGN_OR_RETURN(std::unique_ptr<DistributedPlan> distributed_plan,
                      coordinator->Coordinate(logical_plan));

  PL_ASSIGN_OR_RETURN(std::unique_ptr<distributed::DistributedAnalyzer> analyzer,
                      distributed::DistributedAnalyzer::Create(distributed_state));
  PL_RETURN_IF_ERROR(analyzer->Execute(distributed_plan.get()));
  return distributed_plan;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
