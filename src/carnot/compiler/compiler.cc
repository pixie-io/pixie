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
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_verifier.h"
#include "src/carnot/compiler/string_reader.h"
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
