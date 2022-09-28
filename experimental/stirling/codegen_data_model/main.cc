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
#include <sstream>

#include "absl/strings/str_split.h"
#include "experimental/stirling/codegen_data_model/generator.h"
#include "src/common/base/base.h"

DEFINE_string(schema_file, "", "The path to the schema file.");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);
  std::ifstream t(FLAGS_schema_file);
  std::stringstream buffer;
  buffer << t.rdbuf();
  std::vector<std::string_view> path_components =
      absl::StrSplit(FLAGS_schema_file, "/", absl::SkipEmpty());
  std::cout << "// DO NOT EDIT! Generated from " << FLAGS_schema_file << std::endl;
  std::cout << GenerateCode(path_components.back(), buffer.str());
  return 0;
}
