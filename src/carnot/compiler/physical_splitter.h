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
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

/**
 * @brief This rule inserts a GRPCBridge in front of blocking operators in a graph.
 * The rule finds MemorySources and then iterates through the
 * children until it hits a Blocking Operator or a Sink (which is also a blocking operator).
 *
 * Then the rule will insert a GRPCBridge (GRPCSink -> GRPCSourceGroup) between the parent_op
 * and the blocking op. The resulting IR should contain two subsets now:
 * 1. Where all sources are MemorySources and all sinks are GRPCSinks
 * 2. Where all sources are GRPCSourceGroups and all sinks are MemorySinks
 *
 * TODO(philkuz)(PL-846) support an optimization to remove extraneous GRPCBridge insertions, or
 * pruning them somehow, as described below:
 *
 * Table1
 *  |   \
 *  |    Agg
 *  |   /
 * Join
 *  |
 * Sink
 *
 * can be accurately represented as
 * Table1
 *  |
 * GRPC
 *  |   \
 *  |    Agg
 *  |   /
 * Join
 *  |
 * Sink
 *
 * but the current implementation does
 * Table1
 *  |  \
 *  |   \
 * GRPC  GRPC
 *  |     |
 *  |    Agg
 *  |   /
 * Join
 *  |
 * Sink
 *
 */
class BlockingOperatorGRPCBridgeRule : public Rule {
 public:
  explicit BlockingOperatorGRPCBridgeRule(CompilerState* compiler_state) : Rule(compiler_state) {}

 private:
  StatusOr<bool> Apply(IRNode* ir_node) override;

  /**
   * @brief Recursive function that inserts a GRPCBridge between any
   * child or subsequent child of that op that is blocking. The recursion stops
   * for any child that is blocking.
   *
   * @param op: the operator to apply.,
   * @return StatusOr<bool>: true if a gRPC bridge is built, Errors are stored in Status.
   */
  StatusOr<bool> InsertGRPCBridgeForBlockingChildOperator(OperatorIR* op);

  /**
   * @brief Creates the GRPCBridge.
   *
   * @param parent_op: the parent operator that feeds into the new GRPCSink.
   * @param child_op: the child operator who's new parent should be the GRPCSourceGroup.
   * @return Status
   */
  Status AddNewGRPCNodes(OperatorIR* parent_op, OperatorIR* child_op);
  int64_t grpc_id_counter_ = 0;
};
}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
