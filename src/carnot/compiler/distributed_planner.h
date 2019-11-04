#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/compiler/rule_executor.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

using distributedpb::CarnotInfo;
/**
 * @brief The planner takes in a logical plan and knowledge about the Machines available for
 * exeuction to create a plan that is close to what is actually executed on the nodes.
 *
 * The distributed plan maps identifiers of nodes to the Plan that corresponds to that node.
 *
 * Distributed planning occurs through the following steps:
 * 0. Planner initialized with the DistributedState
 * 1. Planner receives the logical plan.
 * 2. Split the logical plan into the Agent and Kelvin components.
 * 3. Layout the distributed plan (create the distributed plan dag).
 * 4. Prune extraneous edges.
 * 5. Return the mapping from distributed_node_id to the distributed plan for that node.
 *
 */
class DistributedPlanner : public NotCopyable {
 public:
  /**
   * @brief The Creation function for the planner.
   *
   * @return StatusOr<std::unique_ptr<DistributedPlanner>>: the distributed planner object or an
   * error.
   */
  static StatusOr<std::unique_ptr<DistributedPlanner>> Create();

  /**
   * @brief Takes in a logical plan and outputs the distributed plan.
   *
   * @param distributed_state: the distributed layout of the vizier instance.
   * @param compiler_state: informastion passed to the compiler.
   * @param logical_plan
   * @return StatusOr<std::unique_ptr<DistributedPlan>>
   */
  StatusOr<std::unique_ptr<DistributedPlan>> Plan(
      const distributedpb::DistributedState& distributed_state, CompilerState* compiler_state,
      const IR* logical_plan);

 private:
  DistributedPlanner() {}

  Status Init();
};

class NoKelvinPlanner : public NotCopyable {
 public:
  /**
   * @brief The Creation function for the planner.
   *
   * @return StatusOr<std::unique_ptr<DistributedPlanner>>: the distributed planner object or an
   * error.
   */
  static StatusOr<std::unique_ptr<NoKelvinPlanner>> Create();

  /**
   * @brief Takes in a logical plan and outputs the distributed plan.
   *
   * @param distributed_state: the distributed layout of the vizier instance.
   * @param compiler_state: informastion passed to the compiler.
   * @param logical_plan
   * @return StatusOr<std::unique_ptr<DistributedPlan>>
   */
  StatusOr<std::unique_ptr<DistributedPlan>> Plan(
      const distributedpb::DistributedState& distributed_state, CompilerState* compiler_state,
      const IR* logical_plan);

 private:
  NoKelvinPlanner() {}
};
}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
