#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/physical_coordinator.h"
#include "src/carnot/compiler/physical_planner.h"
#include "src/carnot/compiler/physical_stitcher.h"
#include "src/carnot/compiler/rules.h"
namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

StatusOr<std::unique_ptr<PhysicalPlanner>> PhysicalPlanner::Create() {
  std::unique_ptr<PhysicalPlanner> planner(new PhysicalPlanner());
  PL_RETURN_IF_ERROR(planner->Init());
  return planner;
}

Status PhysicalPlanner::Init() { return Status::OK(); }

StatusOr<std::unique_ptr<PhysicalPlan>> PhysicalPlanner::Plan(
    const compilerpb::PhysicalState& physical_state, CompilerState* compiler_state,
    const IR* logical_plan) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Coordinator> coordinator,
                      Coordinator::Create(physical_state));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Stitcher> stitcher, Stitcher::Create(compiler_state));

  PL_ASSIGN_OR_RETURN(std::unique_ptr<PhysicalPlan> physical_plan,
                      coordinator->Coordinate(logical_plan));
  PL_RETURN_IF_ERROR(stitcher->Stitch(physical_plan.get()));

  return physical_plan;
}

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
