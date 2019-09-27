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

class PypaErrorHandler {
 public:
  /**
   * @brief The call back function to the error handler.
   *
   * @param err
   */
  void HandlerFunc(pypa::Error err) { errs_.push_back(err); }

  /**
   *
   * @brief Returns the errors as a status that can then be read by dependent functions.
   *
   * @return Status
   */
  Status ProcessErrors() {
    compilerpb::CompilerErrorGroup error_group;
    for (const auto& err : errs_) {
      compilerpb::CompilerError* err_pb = error_group.add_errors();
      compilerpb::LineColError* lc_err_pb = err_pb->mutable_line_col_error();
      CreateLineColError(lc_err_pb, err);
    }
    return Status(statuspb::INVALID_ARGUMENT, "",
                  std::make_unique<compilerpb::CompilerErrorGroup>(error_group));
  }

 private:
  void CreateLineColError(compilerpb::LineColError* line_col_err_pb, pypa::Error err) {
    int64_t line = err.cur.line;
    int64_t column = err.cur.column;
    std::string error_name;
    switch (err.type) {
      case pypa::ErrorType::IndentationError:
        error_name = "IndentationError:";
        break;
      case pypa::ErrorType::SyntaxError:
        error_name = "SyntaxError:";
        break;
      case pypa::ErrorType::SyntaxWarning:
        error_name = "SyntaxWarning:";
        break;
      default:
        error_name = "";
    }
    std::string message = absl::Substitute("$0 $1", error_name, err.message);

    line_col_err_pb->set_line(line);
    line_col_err_pb->set_column(column);
    line_col_err_pb->set_message(message);
  }

  std::vector<pypa::Error> errs_;
};

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query,
                                                  CompilerState* compiler_state) {
  if (query.empty()) {
    return error::InvalidArgument("Query should not be empty.");
  }

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTWalker ast_walker(ir, compiler_state);

  PypaErrorHandler pypa_error_handler;
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;

  options.error_handler =
      std::bind(&PypaErrorHandler::HandlerFunc, &pypa_error_handler, std::placeholders::_1);
  pypa::Lexer lexer(std::make_unique<StringReader>(query));

  Status result;
  if (pypa::parse(lexer, ast, symbols, options)) {
    result = ast_walker.ProcessModuleNode(ast);
  } else {
    // result = error::InvalidArgument("Parsing was unsuccessful, likely because of broken
    // argument.");
    result = pypa_error_handler.ProcessErrors();
  }
  PL_RETURN_IF_ERROR(result);
  return ir;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
