#include <chrono>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_relation_handler.h"
#include "src/carnot/compiler/ir_verifier.h"
#include "src/carnot/compiler/optimize_ir.h"
#include "src/carnot/compiler/string_reader.h"
#include "src/carnot/planpb/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {
StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query, compiler_state));
  PL_RETURN_IF_ERROR(VerifyIRConnections(*ir));
  PL_RETURN_IF_ERROR(UpdateColumnsAndVerifyUDFs(ir.get(), compiler_state));
  PL_RETURN_IF_ERROR(OptimizeIR(ir.get()));
  return IRToLogicalPlan(*ir);
}
Status Compiler::VerifyIRConnections(const IR& ir) {
  auto verifier = IRVerifier();
  PL_RETURN_IF_ERROR(verifier.VerifyGraphConnections(ir));
  PL_RETURN_IF_ERROR(verifier.VerifyLineColGraph(ir));
  return Status::OK();
}
Status Compiler::UpdateColumnsAndVerifyUDFs(IR* ir, CompilerState* compiler_state) {
  auto relation_handler = IRRelationHandler(compiler_state);
  return relation_handler.UpdateRelationsAndCheckFunctions(ir);
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query,
                                                  CompilerState* compiler_state) {
  if (query.empty()) {
    return error::InvalidArgument("Query should not be empty.");
  }

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTWalker ast_walker(ir, compiler_state);

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

StatusOr<planpb::Plan> Compiler::IRToLogicalPlan(const IR& ir) {
  auto plan = planpb::Plan();
  // TODO(michelle) For M1.5 , we'll only handle plans with a single plan fragment. In the future
  // we will need to update this to loop through all plan fragments.
  auto plan_dag = plan.mutable_dag();
  auto plan_dag_node = plan_dag->add_nodes();
  plan_dag_node->set_id(1);

  auto plan_fragment = plan.add_nodes();
  plan_fragment->set_id(1);
  auto plan_fragment_dag = plan_fragment->mutable_dag();

  auto s = IRWalker()
               .OnMemorySink([&](const auto& mem_sink) {
                 return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, mem_sink);
               })
               .OnMemorySource([&](const auto& mem_src) {
                 return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, mem_src);
               })
               .OnMap([&](const auto& map) {
                 return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, map);
               })
               .OnBlockingAggregate([&](const auto& agg) {
                 return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, agg);
               })
               .OnFilter([&](const auto& filter) {
                 return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, filter);
               })
               .OnLimit([&](const auto& limit) {
                 return IRNodeToPlanNode(plan_fragment, plan_fragment_dag, ir, limit);
               })
               .Walk(ir);
  PL_RETURN_IF_ERROR(s);
  return plan;
}

Status Compiler::OptimizeIR(IR* ir) {
  IROptimizer optimizer;
  PL_RETURN_IF_ERROR(optimizer.Optimize(ir));
  return Status::OK();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
