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

#include "experimental/stirling/codegen_data_model/generator.h"

#include <vector>

#include "absl/strings/str_split.h"
#include "src/common/base/base.h"

// Format of the schema file
// <cpp type> <cpp variable>
// ...
//
// For a schema file name.schema, Will generate one name.h file with the same name as the schema
// file. It includes the following:
// * A struct named after the schema file, with the same fields copied from schema file to struct.
// * A function to copy that record to record batch.
// * A function to return a vector of DataElement.

struct Variable {
  std::string_view type;
  std::string_view name;
};

std::string GenerateCode(std::string_view record_name, std::string_view text) {
  std::vector<std::string_view> lines = absl::StrSplit(text, "\n", absl::SkipWhitespace());
  std::vector<Variable> vars;
  for (std::string_view line : lines) {
    std::vector<std::string_view> fields = absl::StrSplit(line, " ", absl::SkipWhitespace());
    CHECK(fields.size() == 2) << "There should be exactly 2 columns";
    vars.push_back(Variable{fields[0], fields[1]});
  }

  std::string result;
  absl::StrAppend(&result, absl::Substitute(R"(#pragma once

#include <string>
#include <utility>
#include <vector>

#include "src/shared/types/column_wrapper.h"
#include "src/stirling/types.h"

namespace px {
namespace stirling {

// Auto generated struct for $0.
struct $0 {
)",
                                            record_name));

  for (const auto& var : vars) {
    absl::StrAppend(&result, absl::Substitute("  $0 $1;\n", var.type, var.name));
  }
  absl::StrAppend(&result, "};\n");

  absl::StrAppend(&result, absl::Substitute(R"(
// Auto generated function for writing $0 to record batch.
inline void Consume$0($0 record, types::ColumnWrapperRecordBatch* record_batch) {
  const uint64_t kExpectedElementCount = $1;
  CHECK(record_batch->size() == kExpectedElementCount)
      << absl::StrFormat("HTTP trace record field count should be: %d, got %d",
                         kExpectedElementCount, record_batch->size());
  auto& columns = *record_batch;

  uint32_t idx = 0;
)",
                                            record_name, vars.size()));

  auto type_from_str = [](std::string_view str) -> std::string_view {
    if (absl::StrContains(str, "int")) {
      return "Int64Value";
    } else if (absl::StrContains(str, "time")) {
      return "Time64NSValue";
    } else if (absl::StrContains(str, "string")) {
      return "StringValue";
    } else {
      CHECK(false) << "Cannot detect type for string: " << str;
    }
  };
  for (const auto& var : vars) {
    std::string_view type = type_from_str(var.type);
    if (type == "StringValue") {
      absl::StrAppend(
          &result, absl::Substitute("  columns[idx++]->Append<types::$0>(std::move(record.$1));\n",
                                    type_from_str(var.type), var.name));
    } else {
      absl::StrAppend(&result, absl::Substitute("  columns[idx++]->Append<types::$0>(record.$1);\n",
                                                type_from_str(var.type), var.name));
    }
  }
  absl::StrAppend(&result, "}\n");

  absl::StrAppend(&result, absl::Substitute(R"(
// Auto generated function to get DataElement objects for $0.
inline std::vector<DataElement> $0Elements() {
  return {
)",
                                            record_name));

  auto data_type_from_str = [](std::string_view str) -> std::string_view {
    if (absl::StrContains(str, "int")) {
      return "INT64";
    } else if (absl::StrContains(str, "time")) {
      return "TIME64NS";
    } else if (absl::StrContains(str, "string")) {
      return "STRING";
    } else {
      CHECK(false) << "Cannot detect type for string: " << str;
    }
  };
  for (const auto& var : vars) {
    absl::StrAppend(&result, absl::Substitute("      DataElement(\"$0\", types::DataType::$1),\n",
                                              var.name, data_type_from_str(var.type)));
  }
  absl::StrAppend(&result, R"(  };
}

}  // namespace stirling
}  // namespace px
)");

  return result;
}
