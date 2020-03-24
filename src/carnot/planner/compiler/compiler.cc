#include <chrono>
#include <memory>
#include <regex>
#include <sole.hpp>
#include <string>
#include <utility>
#include <vector>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state,
                                         const ArgValues& arg_values) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, CompileToIR(query, compiler_state, arg_values));
  return ir->ToProto();
}

StatusOr<std::shared_ptr<IR>> Compiler::CompileToIR(const std::string& query,
                                                    CompilerState* compiler_state,
                                                    const ArgValues& arg_values) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query, compiler_state, arg_values));
  PL_RETURN_IF_ERROR(Analyze(ir.get(), compiler_state));

  PL_RETURN_IF_ERROR(VerifyGraphHasMemorySink(ir.get()));
  return ir;
}

Status Compiler::Analyze(IR* ir, CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer, Analyzer::Create(compiler_state));
  return analyzer->Execute(ir);
}

StatusOr<shared::scriptspb::FuncArgsSpec> Compiler::GetMainFuncArgsSpec(
    const std::string& query, CompilerState* compiler_state) {
  // TODO(nserrino): PL-1578 remove this after UI queries are updated.
  // This should be ok because calling "import px" multiple times in the same script is ok,
  // both in our system and in Python.
  auto with_import_px = "import px\n" + query;
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(with_import_px));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  PL_ASSIGN_OR_RETURN(auto ast_walker, ASTVisitorImpl::Create(ir.get(), compiler_state, {}));
  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));

  return ast_walker->GetMainFuncArgsSpec();
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query,
                                                  CompilerState* compiler_state,
                                                  const ArgValues& arg_values) {
  // TODO(nserrino): PL-1578 remove this after UI queries are updated.
  // This should be ok because calling "import px" multiple times in the same script is ok,
  // both in our system and in Python.
  auto with_import_px = "import px\n" + query;

  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(with_import_px));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  PL_ASSIGN_OR_RETURN(auto ast_walker,
                      ASTVisitorImpl::Create(ir.get(), compiler_state, arg_values));

  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  return ir;
}

StatusOr<pl::shared::scriptspb::VizFuncsInfo> Compiler::GetVizFuncsInfo(
    const std::string& query, CompilerState* compiler_state) {
  // TODO(nserrino): PL-1578 remove this after UI queries are updated.
  // This should be ok because calling "import px" multiple times in the same script is ok,
  // both in our system and in Python.
  auto with_import_px = "import px\n" + query;

  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(with_import_px));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  PL_ASSIGN_OR_RETURN(auto ast_walker, ASTVisitorImpl::Create(ir.get(), compiler_state, {}));
  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  return ast_walker->GetVizFuncsInfo();
}

Status Compiler::VerifyGraphHasMemorySink(IR* ir) {
  auto sinks = ir->GetSinks();
  if (sinks.size() == 0) {
    return error::InvalidArgument("query does not output a result, please add a $0.$1() statement",
                                  PixieModule::kPixieModuleObjName, PixieModule::kDisplayOpId);
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
