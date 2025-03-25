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

#include "src/common/base/base.h"
#include "src/common/base/env.h"
#include "src/common/json/json.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"

using px::StatusOr;
using px::stirling::obj_tools::ArgInfo;
using px::stirling::obj_tools::DwarfReader;
using px::stirling::obj_tools::ElfReader;
using px::utils::ToJSONString;

//-----------------------------------------------------------------------------
// This utility is designed to isolate parsing the debug symbols of a Go binary. This
// verifies that the go version detection code is functioning as well. This is useful
// for debugging when the Go elf/DWARF parsing is not working correctly and has been the
// source of a few PEM crashes (gh#1300, gh#1646). This makes it easy for asking end users to run
// against their binaries when they are sensitive (proprietary) and we can't debug them ourselves.
//-----------------------------------------------------------------------------

DEFINE_string(binary, "", "The binary to parse. Required argument");
DEFINE_string(
    func_names, "",
    "Comma separated list of function args to parse. Required argument. Example: foo,bar");
DEFINE_bool(json_output, true, "Whether to use JSON output when printing to stdout");

void PrintArgsAsJSON(const std::map<std::string, ArgInfo>& args) {
  px::utils::JSONObjectBuilder builder;
  for (const auto& [name, arg] : args) {
    builder.WriteKVRecursive(name, arg);
  }
  std::cout << builder.GetString() << "\n";
}

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  if (FLAGS_binary.empty() || FLAGS_func_names.empty()) {
    LOG(FATAL) << absl::Substitute(
        "Expected --binary and --func_names arguments to be provided. Instead received $0", *argv);
  }

  StatusOr<std::unique_ptr<ElfReader>> elf_reader_status = ElfReader::Create(FLAGS_binary);
  if (!elf_reader_status.ok()) {
    LOG(WARNING) << absl::Substitute(
        "Failed to parse elf binary $0 with"
        "Message = $1",
        FLAGS_binary, elf_reader_status.msg());
  }
  std::unique_ptr<ElfReader> elf_reader = elf_reader_status.ConsumeValueOrDie();

  StatusOr<std::unique_ptr<DwarfReader>> dwarf_reader_status =
      DwarfReader::CreateIndexingAll(FLAGS_binary);
  if (!dwarf_reader_status.ok()) {
    VLOG(1) << absl::Substitute(
        "Failed to get binary $0 debug symbols. "
        "Message = $1",
        FLAGS_binary, dwarf_reader_status.msg());
  }
  std::unique_ptr<DwarfReader> dwarf_reader = dwarf_reader_status.ConsumeValueOrDie();

  for (const auto& func_name : absl::StrSplit(FLAGS_func_names, ',')) {
    if (func_name.empty()) {
      LOG(FATAL) << absl::Substitute("Empty function name provided in --func_names: $0",
                                     FLAGS_func_names);
    }
    auto args_or_status = dwarf_reader->GetFunctionArgInfo(func_name);

    if (!args_or_status.ok()) {
      LOG(ERROR) << absl::Substitute("debug symbol parsing failed with: $0", args_or_status.msg());
    }
    auto args = args_or_status.ConsumeValueOrDie();
    if (!FLAGS_json_output) {
      for (const auto& [name, arg] : args) {
        LOG(INFO) << absl::Substitute("Arg $0: $1", name, arg.ToString());
      }
    } else {
      PrintArgsAsJSON(args);
    }
  }
}
