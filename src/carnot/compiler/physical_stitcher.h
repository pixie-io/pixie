#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/physical_coordinator.h"
#include "src/carnot/compiler/physical_plan.h"
#include "src/carnot/compiler/physical_splitter.h"
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

using compilerpb::CarnotInfo;

/**
 * @brief The stitcher takes in a carnot graph and a split_plan, creates the physical plan and
 * handles all the appropritate connections.
 *
 */
class Stitcher : public NotCopyable {
 public:
  static StatusOr<std::unique_ptr<Stitcher>> Create(CompilerState* compiler_state);
  /**
   * @brief Takes in a physical_plan that has been assembled, stitches the internal plans together
   * (ie associate GRPCSinks to Sources), and finalizes the plans for execution.
   *
   * @param physical_plan: assembled plan, but not yet stitched.
   * @return Status any errors that occur during the stiching.
   */
  Status Stitch(PhysicalPlan* physical_plan);

 private:
  explicit Stitcher(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  /**
   * @brief Associates the nodes on each edge of the PhysicalPlan with one another.
   *
   * @param plan
   * @return Status
   */
  Status AssociateEdges(PhysicalPlan* plan);

  /**
   * @brief Prepare physical plan before associating edges.
   *
   * @param plan
   * @return Status
   */
  Status PreparePhysicalPlan(PhysicalPlan* plan);

  /**
   * @brief Sets the GRPC address for the GRPC Source Group on a graph.
   *
   * @param graph: the carnot instance to update.
   * @return Status
   */
  Status SetSourceGroupGRPCAddress(CarnotInstance* carnot_instance);

  /**
   * @brief Connects the graphs on two Carnot instances by doing the following:
   * 1. Associate GRPCSinks in from_graph to GRPCSourceGroups in to_graph.
   *
   * @param from_graph: the from node on this edge
   * @param to_graph: the to node on this edge.
   * @return Status
   */
  Status ConnectGraphs(IR* from_graph, IR* to_graph);

  /**
   * @brief Finalize the passed in graph for execution by doing the following:
   * 1. Converts GRPCSourceGroups to GRPCSource and Unions
   * 2. Checks to make sure that only physical nodes are leftover.
   * 3. Prune any extra nodes in the plan (ie due to Filters).
   *
   * @param graph
   * @return Status
   */
  Status FinalizeGraph(IR* graph);

  Status FinalizePlan(PhysicalPlan* plan);

  CompilerState* compiler_state_;
};

class SetSourceGroupGRPCAddressRule : public Rule {
 public:
  explicit SetSourceGroupGRPCAddressRule(const std::string& grpc_address,
                                         const std::string& query_broker_address)
      : Rule(nullptr), grpc_address_(grpc_address), query_broker_address_(query_broker_address) {}

 private:
  StatusOr<bool> Apply(IRNode* node) override;
  std::string grpc_address_;
  std::string query_broker_address_;
};

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
