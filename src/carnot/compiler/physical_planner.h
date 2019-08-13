#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/physical_plan.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/compiler/rule_executor.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

/**
 * @brief The planner takes in a logical plan and knowledge about the Machines available for
 * exeuction to create a plan that is close to what is actually executed on the nodes.
 *
 * The physical plan maps identifiers of nodes to the Plan that corresponds to that node.
 *
 * Physical planning occurs through the following steps:
 * 0. Planner initialized with the PhysicalState
 * 1. Planner receives the logical plan.
 * 2. Split the logical plan into the Agent and Kelvin components.
 * 3. Layout the physical plan (create the physical plan dag).
 * 4. Prune extraneous edges.
 * 5. Return the mapping from physical_node_id to the physical plan for that node.
 *
 */
class PhysicalPlanner : public NotCopyable {
 public:
  static StatusOr<std::unique_ptr<PhysicalPlanner>> Create(
      const compilerpb::PhysicalState& physical_state) {
    std::unique_ptr<PhysicalPlanner> planner(new PhysicalPlanner(physical_state));
    PL_RETURN_IF_ERROR(planner->Init());
    return planner;
  }
  StatusOr<std::unique_ptr<PhysicalPlan>> Plan(const IR* logical_plan);

 private:
  explicit PhysicalPlanner(const compilerpb::PhysicalState& physical_state)
      : physical_state_(physical_state) {}

  // TODO(philkuz) decide whether to keept his around.
  Status Init() { return Status::OK(); }

  compilerpb::PhysicalState physical_state_;
};
}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
