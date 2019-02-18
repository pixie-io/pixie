#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/string_reader.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {
StatusOr<carnotpb::Plan> Compiler::Compile(const std::string& query,
                                           CompilerState* compiler_state) {
  PL_UNUSED(compiler_state);
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query));
  return IRToLogicalPlan(ir);
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

StatusOr<carnotpb::Plan> Compiler::IRToLogicalPlan(std::shared_ptr<IR>) { return carnotpb::Plan(); }

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
