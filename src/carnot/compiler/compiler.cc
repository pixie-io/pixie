#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_relation_handler.h"
#include "src/carnot/compiler/ir_verifier.h"
#include "src/carnot/compiler/string_reader.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/common/time.h"

namespace pl {
namespace carnot {
namespace compiler {
StatusOr<carnotpb::Plan> Compiler::Compile(const std::string& query,
                                           CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query));
  PL_RETURN_IF_ERROR(VerifyIRConnections(*ir));
  PL_RETURN_IF_ERROR(UpdateColumnsAndVerifyUDFs(ir.get(), compiler_state));
  return IRToLogicalPlan(*ir);
}
Status Compiler::VerifyIRConnections(const IR& ir) {
  auto verifier = IRVerifier();
  std::vector<Status> result = verifier.VerifyGraphConnections(ir);
  if (result.size() != 0) {
    std::vector<std::string> msgs;
    error::Code code = error::Code::OK;

    for (const auto& err : result) {
      msgs.push_back(err.ToString());
      code = err.code();
    }
    return Status(code, absl::StrJoin(msgs, "\n"));
  }
  return Status::OK();
}
Status Compiler::UpdateColumnsAndVerifyUDFs(IR* ir, CompilerState* compiler_state) {
  // TODO(philkuz) fix the compiler state schema or IRelationHandler to uncomment the following
  // lines.
  auto relation_handler =
      IRRelationHandler(*compiler_state->relation_map(), *compiler_state->registry_info());
  return relation_handler.UpdateRelationsAndCheckFunctions(ir);
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query) {
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTWalker ast_walker(ir);

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  pypa::Lexer lexer(std::make_unique<StringReader>(query));

  Status result;
  if (pypa::parse(lexer, ast, symbols, options)) {
    result = ast_walker.ProcessModuleNode(ast);
  } else {
    result = error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }
  PL_RETURN_IF_ERROR(result);
  return ir;
}

StatusOr<carnotpb::Plan> Compiler::IRToLogicalPlan(const IR& ir) {
  auto plan = carnotpb::Plan();
  // TODO(michelle) For M1.5 , we'll only handle plans with a single plan fragment. In the future
  // we will need to update this to loop through all plan fragments.
  auto plan_dag = plan.mutable_dag();
  auto plan_dag_node = plan_dag->add_nodes();
  plan_dag_node->set_id(1);

  auto plan_fragment = plan.add_nodes();
  plan_fragment->set_id(1);
  auto plan_fragment_dag = plan_fragment->mutable_dag();

  IRWalker()
      .OnMemorySink([&](const auto& mem_sink) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, mem_sink);
      })
      .OnMemorySource([&](const auto& mem_src) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, mem_src);
      })
      .OnMap([&](const auto& map) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, map);
      })
      .OnAgg([&](const auto& agg) {
        return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, agg);
      })
      .Walk(ir);
  return plan;
}

Status Compiler::OptimizeIR(IR* ir) {
  // Collapse Range into From.
  PL_RETURN_IF_ERROR(CollapseRange(ir));
  return Status::OK();
}

Status Compiler::CollapseRange(IR* ir) {
  auto dag = ir->dag();
  auto sorted_dag = dag.TopologicalSort();

  // This assumes there is only one Range in the query.
  RangeIR* rangeIR = nullptr;
  MemorySourceIR* srcIR;
  StringIR* timeIR;
  for (const auto& node_id : sorted_dag) {
    auto node = ir->Get(node_id);
    if (node->type() == IRNodeType::RangeType) {
      rangeIR = static_cast<RangeIR*>(node);

      // Assumes that range directly follows memory source.
      srcIR = static_cast<MemorySourceIR*>(rangeIR->parent());
      ir->DeleteEdge(srcIR->id(), rangeIR->id());

      timeIR = static_cast<StringIR*>(rangeIR->time_repr());
      auto now = std::chrono::high_resolution_clock::now();
      auto now_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      PL_ASSIGN_OR_RETURN(auto time_difference, StringToTimeInt(timeIR->str()));
      srcIR->SetTime(now_ms + time_difference, now_ms);
      ir->DeleteNode(timeIR->id());

      // Update all of range's dependencies to point to src.
      for (const auto& dep_id : ir->dag().DependenciesOf(rangeIR->id())) {
        auto dep = ir->Get(dep_id);
        ir->DeleteEdge(rangeIR->id(), dep_id);
        PL_RETURN_IF_ERROR(ir->AddEdge(srcIR->id(), dep_id));
        if (dep->IsOp()) {
          auto casted_node = static_cast<OperatorIR*>(dep);
          PL_RETURN_IF_ERROR(casted_node->SetParent(dynamic_cast<IRNode*>(srcIR)));
        }
      }

      break;
    }
  }
  if (rangeIR) {
    ir->DeleteNode(rangeIR->id());
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
