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

#pragma once
#include <string>
#include <vector>

#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {

struct DisplayLine {
  std::string line;
  std::string table_name;
  std::string table_argument;
  // The zero-indexed line number in the script where the px.display() call starts.
  int64_t line_number_start;
  // The zero-indexed line number in the script where the px.display() call ends.
  // If the call is on a single line, then the end line number is the same as the start line
  // number.
  int64_t line_number_end;
};

struct DataFrameCall {
  // The original lines of the px.DataFrame call.
  std::string original_call;
  // The updated line after the px.DataFrame call has been rewritten.
  std::string updated_line;
  // The zero-indexed line number in the script where the px.DataFrame() call starts.
  int64_t line_number_start;
  // The zero-indexed line number in the script where the px.DataFrame() call ends.
  // If the call is on a single line, then the end line number is the same as the start line
  // number.
  int64_t line_number_end;
};

class OTelGenerator {
 public:
  static StatusOr<std::string> GenerateOTelScript(compiler::Compiler* compiler,
                                                  CompilerState* compiler_state,
                                                  const std::string& pxl_script);

  static StatusOr<std::string> GetUnusedVarName(CompilerState* compiler_state,
                                                const std::string& script,
                                                const std::string& base_name);
  static StatusOr<std::vector<DisplayLine>> GetPxDisplayLines(const std::string& script);
  static StatusOr<absl::flat_hash_map<std::string, ::px::table_store::schemapb::Relation>>
  CalculateOutputSchemas(compiler::Compiler* compiler, CompilerState* compiler_state,
                         const std::string& pxl_script);
  static StatusOr<std::string> RelationToOTelExport(
      const std::string& table_name, const std::string& unique_df_name,
      const px::table_store::schemapb::Relation& relation);

  static StatusOr<std::vector<DataFrameCall>> ReplaceDataFrameTimes(const std::string& script);
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
