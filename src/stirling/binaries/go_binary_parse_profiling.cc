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
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"

using px::StatusOr;
using px::stirling::PopulateGoTLSDebugSymbols;
using px::stirling::obj_tools::DwarfReader;
using px::stirling::obj_tools::ElfReader;

//-----------------------------------------------------------------------------
// This utility is designed to isolate parsing the debug symbols of a Go binary. This
// verifies that the go version detection code is functioning as well. This is useful
// for debugging when the Go elf/DWARF parsing is not working correctly and has been the
// source of a few PEM crashes (gh#1300, gh#1646). This makes it easy for asking end users to run against
// their binaries when they are sensitive (proprietary) and we can't debug them ourselves.
//-----------------------------------------------------------------------------

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  if (argc < 2) {
    LOG(FATAL) << absl::Substitute("Expected binary argument to be provided. Instead received $0",
                                   *argv);
  }

  std::string binary(argv[1]);

  StatusOr<std::unique_ptr<ElfReader>> elf_reader_status = ElfReader::Create(binary);
  if (!elf_reader_status.ok()) {
    LOG(WARNING) << absl::Substitute(
        "Failed to parse elf binary $0 with"
        "Message = $1",
        binary, elf_reader_status.msg());
  }
  std::unique_ptr<ElfReader> elf_reader = elf_reader_status.ConsumeValueOrDie();

  StatusOr<std::unique_ptr<DwarfReader>> dwarf_reader_status =
      DwarfReader::CreateIndexingAll(binary);
  if (!dwarf_reader_status.ok()) {
    VLOG(1) << absl::Substitute(
        "Failed to get binary $0 debug symbols. "
        "Message = $1",
        binary, dwarf_reader_status.msg());
  }
  std::unique_ptr<DwarfReader> dwarf_reader = dwarf_reader_status.ConsumeValueOrDie();

  struct go_tls_symaddrs_t symaddrs;
  auto status = PopulateGoTLSDebugSymbols(elf_reader.get(), dwarf_reader.get(), &symaddrs);

  if (!status.ok()) {
    LOG(ERROR) << absl::Substitute("debug symbol parsing failed with: $0", status.msg());
  }
}
