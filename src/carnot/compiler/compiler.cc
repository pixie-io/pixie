#include <chrono>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/analyzer.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir_verifier.h"
#include "src/carnot/compiler/parser/parser.h"
#include "src/carnot/planpb/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {
StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, CompileToIR(query, compiler_state));
  return ir->ToProto();
}

StatusOr<std::shared_ptr<IR>> Compiler::CompileToIR(const std::string& query,
                                                    CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query, compiler_state));
  PL_RETURN_IF_ERROR(VerifyIRConnections(*ir));
  PL_RETURN_IF_ERROR(UpdateColumnsAndVerifyUDFs(ir.get(), compiler_state));
  return ir;
}
Status Compiler::VerifyIRConnections(const IR& ir) {
  auto verifier = IRVerifier();
  PL_RETURN_IF_ERROR(verifier.VerifyGraphConnections(ir));
  PL_RETURN_IF_ERROR(verifier.VerifyLineColGraph(ir));
  return Status::OK();
}
Status Compiler::UpdateColumnsAndVerifyUDFs(IR* ir, CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer, Analyzer::Create(compiler_state));
  return analyzer->Execute(ir);
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query,
                                                  CompilerState* compiler_state) {
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTVisitorImpl ast_walker(ir.get(), compiler_state);

  PL_RETURN_IF_ERROR(ast_walker.ProcessModuleNode(ast));
  return ir;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
