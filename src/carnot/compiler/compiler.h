#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {

/**
 * The compiler takes a query in the form of a string and compiles it into a logical plan.
 */
class Compiler {
 public:
  /**
   * Compile the query into a logical plan.
   * @param query the query to compile.
   * @return the logical plan in the form of a plan protobuf message.
   */
  StatusOr<carnotpb::Plan> Compile(const std::string& query, CompilerState* compiler_state);

  StatusOr<carnotpb::Plan> IRToLogicalPlan(const IR& ir);

  static Status CollapseRange(IR* ir);

 private:
  StatusOr<std::shared_ptr<IR>> QueryToIR(const std::string& query);

  template <typename TIRNode>
  Status IRNodeToPlanNode(carnotpb::PlanFragment* pf, carnotpb::DAG* pf_dag, const IR& ir_graph,
                          const TIRNode& ir_node) {
    // Add PlanNode.
    auto plan_node = pf->add_nodes();
    plan_node->set_id(ir_node.id());
    auto op_pb = plan_node->mutable_op();
    PL_RETURN_IF_ERROR(ir_node.ToProto(op_pb));

    // Add DAGNode.
    auto dag_node = pf_dag->add_nodes();
    dag_node->set_id(ir_node.id());
    for (const auto& dep : ir_graph.dag().DependenciesOf(ir_node.id())) {
      // Only add dependencies for operator IR nodes.
      if (ir_graph.Get(dep)->IsOp()) {
        dag_node->add_sorted_deps(dep);
      }
    }
    return Status::OK();
  }
  /**
   * Optimize the query by updating the IR. This may mutate the IR, such as updating nodes,
   * removing nodes or adding/removing edges.
   * @param ir the ir to optimize
   * @return a status of whether optimization was successful.
   */
  Status OptimizeIR(IR* ir);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
