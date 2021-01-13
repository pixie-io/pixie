#include "src/carnot/planner/probes/tracepoint_generator.h"

#include <fstream>
#include <iostream>
#include <memory>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/probes/probes.h"

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<stirling::dynamic_tracing::ir::logical::TracepointDeployment> CompileTracepoint(
    std::string_view query) {
  // Create a dummy compiler state; it doesn't affect the tracepoint compilation.
  // TODO(oazizi): Try inserting nullptr for registry_info.
  pl::carnot::planner::RegistryInfo registry_info;
  pl::carnot::planner::CompilerState dummy_compiler_state(
      std::make_unique<pl::carnot::planner::RelationMap>(), &registry_info,
      // Time now isn't used to generate probes, but we still need to pass one in.
      /*time_now*/ 1552607213931245000,
      /*max_output_rows_per_table*/ 10000, "dummy_result_addr", /* SSL target name override */ "");

  Parser parser;
  PL_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));

  IR ir;
  compiler::MutationsIR probe_ir;
  ModuleHandler module_handler;

  PL_ASSIGN_OR_RETURN(auto ast_walker,
                      compiler::ASTVisitorImpl::Create(&ir, &probe_ir, &dummy_compiler_state,
                                                       &module_handler, false, {}, {}));

  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));

  plannerpb::CompileMutationsResponse pb;
  PL_RETURN_IF_ERROR(probe_ir.ToProto(&pb));
  if (pb.mutations_size() != 1) {
    return error::Internal("Unexpected number of mutations");
  }

  return pb.mutations()[0].trace();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
