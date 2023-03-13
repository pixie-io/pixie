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
  px::EnvironmentGuard env_guard(&argc, argv);
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

  PX_ASSIGN_OR_EXIT(auto tracepoint_pb, px::carnot::planner::compiler::CompileTracepoint(query));
  PX_CHECK_OK(px::WriteFileFromString(output_file, tracepoint_pb.DebugString()));
  return 0;
}
