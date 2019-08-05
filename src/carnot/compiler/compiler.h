#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/planpb/plan.pb.h"

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
  StatusOr<planpb::Plan> Compile(const std::string& query, CompilerState* compiler_state);
  // TODO(philkuz) make irtologicalplan private.
  StatusOr<planpb::Plan> IRToLogicalPlan(const IR& ir);

 private:
  StatusOr<std::shared_ptr<IR>> QueryToIR(const std::string& query, CompilerState* compiler_state);

  template <typename TIRNode>
  Status IRNodeToPlanNode(planpb::PlanFragment* pf, planpb::DAG* pf_dag, const IR& ir_graph,
                          const TIRNode& ir_node) {
    // Check to make sure that the relation is set for this ir_node, otherwise it's not connected to
    // a Sink.
    if (!ir_node.IsRelationInit()) {
      return ir_node.CreateIRNodeError(
          "$0(id=$1) doesn't have a relation. Please remove this call or add "
          "a `$2()` call on this.",
          ir_node.type_string(), ir_node.id(), ASTWalker::kSinkOpId);
    }

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
  Status UpdateColumnsAndVerifyUDFs(IR* ir, CompilerState* compiler_state);
  Status VerifyIRConnections(const IR& ir);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
