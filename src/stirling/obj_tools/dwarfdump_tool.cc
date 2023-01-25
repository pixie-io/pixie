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

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>

#include <string>

#include "src/common/base/base.h"

#include "src/stirling/obj_tools/dwarf_reader.h"

constexpr char kProgramDescription[] =
    "A simple tool that finds debug information in object files with DWARF info.\n"
    "Like dwarfdump, but simplified.";

DEFINE_string(filename, "", "Object file to search.");
DEFINE_string(die_name, "", "The Debugging Information Entry (DIE) to search for.");

void InitDumpOpts(llvm::DIDumpOptions* opts) {
  opts->DumpType = llvm::DIDT_DebugInfo;  // Other options: DIDT_UUID, DIDT_All, DIDT_Null
  opts->ChildRecurseDepth = -1;
  opts->ParentRecurseDepth = -1;
  opts->ShowAddresses = true;
  opts->ShowChildren = true;
  opts->ShowParents = false;
  opts->ShowForm = false;
  opts->SummarizeTypes = false;
  opts->Verbose = false;
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kProgramDescription);
  px::EnvironmentGuard env_guard(&argc, argv);

  std::error_code ec;
  llvm::ToolOutputFile OutputFile("-", ec, llvm::sys::fs::OF_Text);
  if (ec) {
    LOG(ERROR) << absl::Substitute("Unable to open file for writing. msg=$0", ec.message());
    exit(1);
  }

  PX_ASSIGN_OR_EXIT(auto dwarf_reader,
                    px::stirling::obj_tools::DwarfReader::CreateWithoutIndexing(FLAGS_filename));
  PX_ASSIGN_OR_EXIT(std::vector<llvm::DWARFDie> dies,
                    dwarf_reader->GetMatchingDIEs(FLAGS_die_name));

  llvm::DIDumpOptions dump_opts;
  InitDumpOpts(&dump_opts);
  for (const auto& d : dies) {
    d.dump(OutputFile.os(), 0, dump_opts);
  }

  return 0;
}
