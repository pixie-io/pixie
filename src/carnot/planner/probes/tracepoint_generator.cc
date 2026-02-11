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

#include "src/carnot/planner/probes/tracepoint_generator.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/probes/probes.h"

#include "src/carnot/planner/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<carnot::planner::dynamic_tracing::ir::logical::TracepointDeployment> CompileTracepoint(
    std::string_view query) {
  // Create a compiler state; it doesn't affect the tracepoint compilation.
  // TODO(oazizi): Try inserting nullptr for registry_info.
  px::carnot::planner::RegistryInfo registry_info;
  px::carnot::planner::CompilerState compiler_state(
      std::make_unique<px::carnot::planner::RelationMap>(),
      px::carnot::planner::SensitiveColumnMap{}, &registry_info,
      // Time now isn't used to generate probes, but we still need to pass one in.
      /*time_now*/ 1552607213931245000,
      /*max_output_rows_per_table*/ 10000, "result_addr", /* SSL target name override */ "",
      RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});

  Parser parser;
  PX_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));

  IR ir;
  compiler::MutationsIR probe_ir;
  ModuleHandler module_handler;

  PX_ASSIGN_OR_RETURN(auto ast_walker,
                      compiler::ASTVisitorImpl::Create(&ir, &probe_ir, &compiler_state,
                                                       &module_handler, false, {}, {}));

  PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));

  plannerpb::CompileMutationsResponse pb;
  PX_RETURN_IF_ERROR(probe_ir.ToProto(&pb));
  if (pb.mutations_size() != 1) {
    return error::Internal("Unexpected number of mutations");
  }

  return pb.mutations()[0].trace();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
