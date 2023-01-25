/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <memory>
#include <regex>
#include <sole.hpp>
#include <string>
#include <utility>
#include <vector>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler/optimizer/optimizer.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/scriptspb/scripts.pb.h"

constexpr char kPxlViews[] = (
#include "src/carnot/planner/pxl_lib/pxviews.pxl"
);
namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state) {
  return Compile(query, compiler_state, /* exec_funcs */ {});
}

StatusOr<planpb::Plan> Compiler::Compile(const std::string& query, CompilerState* compiler_state,
                                         const ExecFuncs& exec_funcs) {
  PX_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, CompileToIR(query, compiler_state, exec_funcs));
  return ir->ToProto();
}

StatusOr<std::shared_ptr<IR>> Compiler::CompileToIR(const std::string& query,
                                                    CompilerState* compiler_state) {
  return CompileToIR(query, compiler_state, /* exec_funcs */ {});
}

StatusOr<std::shared_ptr<IR>> Compiler::CompileToIR(const std::string& query,
                                                    CompilerState* compiler_state,
                                                    const ExecFuncs& exec_funcs) {
  PX_ASSIGN_OR_RETURN(std::shared_ptr<IR> ir, QueryToIR(query, compiler_state, exec_funcs));
  PX_RETURN_IF_ERROR(Analyze(ir.get(), compiler_state));
  PX_RETURN_IF_ERROR(Optimize(ir.get(), compiler_state));

  PX_RETURN_IF_ERROR(VerifyGraphHasResultSink(ir.get()));
  return ir;
}

Status Compiler::Analyze(IR* ir, CompilerState* compiler_state) {
  PX_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer, Analyzer::Create(compiler_state));
  return analyzer->Execute(ir);
}

Status Compiler::Optimize(IR* ir, CompilerState* compiler_state) {
  PX_ASSIGN_OR_RETURN(std::unique_ptr<Optimizer> optimizer, Optimizer::Create(compiler_state));
  return optimizer->Execute(ir);
}

StatusOr<std::shared_ptr<IR>> Compiler::QueryToIR(const std::string& query,
                                                  CompilerState* compiler_state,
                                                  const ExecFuncs& exec_funcs) {
  Parser parser;
  PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

  std::shared_ptr<IR> ir = std::make_shared<IR>();
  bool func_based_exec = exec_funcs.size() > 0;
  absl::flat_hash_set<std::string> reserved_names;
  for (const auto& func : exec_funcs) {
    reserved_names.insert(func.output_table_prefix());
  }
  MutationsIR mutations_ir;
  ModuleHandler module_handler;

  absl::flat_hash_map<std::string, std::string> module_map;
  module_map["pxviews"] = kPxlViews;
  PX_ASSIGN_OR_RETURN(auto ast_walker, ASTVisitorImpl::Create(
                                           ir.get(), &mutations_ir, compiler_state, &module_handler,
                                           func_based_exec, reserved_names, module_map));

  PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  if (func_based_exec) {
    PX_RETURN_IF_ERROR(ast_walker->ProcessExecFuncs(exec_funcs));
  }
  return ir;
}

StatusOr<std::unique_ptr<MutationsIR>> Compiler::CompileTrace(const std::string& query,
                                                              CompilerState* compiler_state,
                                                              const ExecFuncs& exec_funcs) {
  Parser parser;
  PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

  IR ir;
  bool func_based_exec = exec_funcs.size() > 0;
  absl::flat_hash_set<std::string> reserved_names;
  for (const auto& func : exec_funcs) {
    reserved_names.insert(func.output_table_prefix());
  }
  std::unique_ptr<MutationsIR> mutations = std::make_unique<MutationsIR>();
  ModuleHandler module_handler;
  PX_ASSIGN_OR_RETURN(auto ast_walker,
                      ASTVisitorImpl::Create(&ir, mutations.get(), compiler_state, &module_handler,
                                             func_based_exec, reserved_names));

  PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  if (func_based_exec) {
    PX_RETURN_IF_ERROR(ast_walker->ProcessExecFuncs(exec_funcs));
  }
  return mutations;
}

Status Compiler::VerifyGraphHasResultSink(IR* ir) {
  auto sinks = ir->FindNodesThatMatch(ResultSink());
  if (sinks.size() == 0) {
    return error::InvalidArgument(
        "query does not output a "
        "result, please add a "
        "$0.$1() statement",
        PixieModule::kPixieModuleObjName, PixieModule::kDisplayOpID);
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
