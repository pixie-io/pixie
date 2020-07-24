#include <fstream>
#include <iostream>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/probes/probes.h"
#include "src/common/base/file.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<DynamicTraceIR>> CompileProbeScript(std::string_view query) {
  std::shared_ptr<DynamicTraceIR> dynamic_trace = std::make_shared<DynamicTraceIR>();

  Parser parser;
  PL_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));

  IR ir;
  ModuleHandler module_handler;

  RegistryInfo registry_info;
  CompilerState compiler_state(
      std::make_unique<RelationMap>(), &registry_info,
      // Time now isn't used to generate probes, but we still need to pass one in.
      /*time_now*/ 1552607213931245000,
      /*max_output_rows_per_table*/ 10000);
  PL_ASSIGN_OR_RETURN(auto ast_walker,
                      ASTVisitorImpl::Create(&ir, dynamic_trace.get(), &compiler_state,
                                             &module_handler, false, {}, {}));
  PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  return dynamic_trace;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl

DEFINE_string(query, "", "The query to run.");
DEFINE_string(
    output_file, "",
    "The file path to write the output protobuf representation of the dynamic tracing protobuf.");

void PrintUsageAndExit(std::string_view name) {
  LOG(ERROR) << absl::Substitute(
      "Usage: $0 --query <query_string> --output_file <file_to_output_protobuf_to>", name);
  exit(1);
}

int main(int argc, char** argv) {
  pl::EnvironmentGuard env_guard(&argc, argv);
  std::string query = FLAGS_query;
  std::string output_file = FLAGS_output_file;
  if (query.empty()) {
    LOG(ERROR) << "Query must not be empty";
    PrintUsageAndExit(argv[0]);
  }

  if (output_file.empty()) {
    LOG(ERROR) << absl::Substitute("Invalid output file: '$0'", output_file);
    PrintUsageAndExit(argv[0]);
  }

  auto probe = pl::carnot::planner::compiler::CompileProbeScript(query).ConsumeValueOrDie();
  pl::carnot::planner::plannerpb::CompileMutationsResponse probe_pb;
  PL_CHECK_OK(probe->ToProto(&probe_pb));
  CHECK_EQ(probe_pb.mutations_size(), 1);
  PL_CHECK_OK(pl::WriteFileFromString(output_file, probe_pb.mutations()[0].trace().DebugString()));
  return 0;
}
