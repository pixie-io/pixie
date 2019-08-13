#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/physical_planner.h"
#include "src/carnot/compiler/rules.h"
namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

StatusOr<std::unique_ptr<PhysicalPlan>> PhysicalPlanner::Plan(const IR* logical_plan) {
  // TODO(philkuz) implement.
  PL_UNUSED(logical_plan);
  PL_UNUSED(physical_state_);
  return std::make_unique<PhysicalPlan>();
}
}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
