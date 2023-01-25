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

#include <memory>

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/elf_reader.h"

DEFINE_string(binary, "", "Filename to list symbols");
DEFINE_string(filter, "", "Symbol matching substring used to select symbols to print.");

using ::px::stirling::obj_tools::ElfReader;
using ::px::stirling::obj_tools::SymbolMatchType;

constexpr char kProgramDescription[] =
    "A tool that lists all the function symbols in a binary (similar to nm).";

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kProgramDescription);
  px::EnvironmentGuard env_guard(&argc, argv);

  if (FLAGS_binary.empty()) {
    LOG(INFO) << absl::Substitute(
        "Usage: $0 --binary <binary_path> [--filter <symbol matching substring>]", argv[0]);
    return 1;
  }

  LOG(INFO) << absl::Substitute("Reading symbols in $0", FLAGS_binary);

  PX_ASSIGN_OR_EXIT(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(FLAGS_binary));
  PX_ASSIGN_OR_EXIT(std::vector<ElfReader::SymbolInfo> symbol_infos,
                    elf_reader->ListFuncSymbols(FLAGS_filter, SymbolMatchType::kSubstr));

  LOG(INFO) << absl::Substitute("Found $0 symbols", symbol_infos.size());

  for (auto& s : symbol_infos) {
    LOG(INFO) << s.name;
  }
}
