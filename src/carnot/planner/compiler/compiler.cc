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
#include "src/carnot/planner/compiler/optimizer/optimizer.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state) {
  return Compile(query, compiler_state, /* exec_funcs */ {});
}

StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state,
                                         const ExecFuncs& exec_funcs) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, CompileToIR(query, compiler_state, exec_funcs));
  return ir->ToProto();
}

StatusOr<std::shared_ptr<IR>> Compiler::CompileToIR(const std::string& query,
                                                    CompilerState* compiler_state) {
  return CompileToIR(query, compiler_state, /* exec_funcs */ {});
}

StatusOr<std::shared_ptr<IR>> Compiler::CompileToIR(const std::string& query,
                                                    CompilerState* compiler_state,
                                                    const ExecFuncs& exec_funcs) {
  PL_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query, compiler_state, exec_funcs));
  PL_RETURN_IF_ERROR(Analyze(ir.get(), compiler_state));
  PL_RETURN_IF_ERROR(Optimize(ir.get(), compiler_state));

  PL_RETURN_IF_ERROR(VerifyGraphHasMemorySink(ir.get()));
  return ir;
}

Status Compiler::Analyze(IR* ir, CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer, Analyzer::Create(compiler_state));
  return analyzer->Execute(ir);
}

Status Compiler::Optimize(IR* ir, CompilerState* compiler_state) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<Optimizer> optimizer, Optimizer::Create(compiler_state));
  return optimizer->Execute(ir);
}

StatusOr<shared::scriptspb::FuncArgsSpec> Compiler::GetMainFuncArgsSpec(
    const std::string& query, CompilerState* compiler_state) {
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ModuleHandler module_handler;
  PL_ASSIGN_OR_RETURN(auto ast_walker,
                      ASTVisitorImpl::Create(ir.get(), compiler_state, &module_handler));
  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));

  return ast_walker->GetMainFuncArgsSpec();
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query,
                                                  CompilerState* compiler_state,
                                                  const ExecFuncs& exec_funcs) {
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  bool func_based_exec = exec_funcs.size() > 0;
  absl::flat_hash_set<std::string> reserved_names;
  for (const auto& func : exec_funcs) {
    reserved_names.insert(func.output_table_prefix());
  }
  ModuleHandler module_handler;
  PL_ASSIGN_OR_RETURN(auto ast_walker,
                      ASTVisitorImpl::Create(ir.get(), compiler_state, &module_handler,
                                             func_based_exec, reserved_names));

  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  if (func_based_exec) {
    PL_RETURN_IF_ERROR(ast_walker->ProcessExecFuncs(exec_funcs));
  }
  return ir;
}

StatusOr<pl::shared::scriptspb::VisFuncsInfo> Compiler::GetVisFuncsInfo(
    const std::string& query, CompilerState* compiler_state) {
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ModuleHandler module_handler;
  PL_ASSIGN_OR_RETURN(auto ast_walker,
                      ASTVisitorImpl::Create(ir.get(), compiler_state, &module_handler));
  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  return ast_walker->GetVisFuncsInfo();
}

Status Compiler::VerifyGraphHasMemorySink(IR* ir) {
  auto sinks = ir->FindNodesThatMatch(MemorySink());
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
