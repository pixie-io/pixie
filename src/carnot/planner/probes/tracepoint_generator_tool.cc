#include <fstream>
#include <iostream>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/probes/tracepoint_generator.h"
#include "src/common/base/file.h"

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

  PL_ASSIGN_OR_EXIT(auto tracepoint_pb, pl::carnot::planner::compiler::CompileTracepoint(query));
  PL_CHECK_OK(pl::WriteFileFromString(output_file, tracepoint_pb.DebugString()));
  return 0;
}
