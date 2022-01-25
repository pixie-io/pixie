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

#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/elf_symbolizer.h"
#include "src/stirling/utils/proc_path_tools.h"

using ::px::stirling::obj_tools::ElfReader;

namespace px {
namespace stirling {

StatusOr<std::unique_ptr<Symbolizer>> ElfSymbolizer::Create() {
  ElfSymbolizer* elf_symbolizer = new ElfSymbolizer();
  auto symbolizer = std::unique_ptr<Symbolizer>(elf_symbolizer);
  return symbolizer;
}

void ElfSymbolizer::DeleteUPID(const struct upid_t& upid) { symbolizers_.erase(upid); }

StatusOr<std::unique_ptr<ElfReader::Symbolizer>> CreateUPIDSymbolizer(const struct upid_t& upid) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<FilePathResolver> fp_resolver,
                      FilePathResolver::Create(upid.pid));
  // TODO(yzhao): Might need to check the start time.
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe,
                      system::ProcParser(system::Config::GetInstance()).GetExePath(upid.pid));
  PL_ASSIGN_OR_RETURN(std::filesystem::path host_proc_exe, fp_resolver->ResolvePath(proc_exe));
  host_proc_exe = system::Config::GetInstance().ToHostPath(host_proc_exe);
  PL_ASSIGN_OR_RETURN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(host_proc_exe));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<ElfReader::Symbolizer> upid_symbolizer,
                      elf_reader->GetSymbolizer());
  return upid_symbolizer;
}

std::string_view EmptySymbolizerFn(const uintptr_t addr) {
  static std::string symbol;
  symbol = absl::StrFormat("0x%016llx", addr);
  return symbol;
}

std::string_view BogusKernelSymbolizerFn(const uintptr_t) { return "<kernel symbol>"; }

profiler::SymbolizerFn ElfSymbolizer::GetSymbolizerFn(const struct upid_t& upid) {
  constexpr uint32_t kKernelPID = static_cast<uint32_t>(-1);
  if (upid.pid == kKernelPID) {
    return profiler::SymbolizerFn(&(BogusKernelSymbolizerFn));
  }

  std::unique_ptr<ElfReader::Symbolizer>& upid_symbolizer = symbolizers_[upid];
  if (upid_symbolizer == nullptr) {
    StatusOr<std::unique_ptr<ElfReader::Symbolizer>> upid_symbolizer_status =
        CreateUPIDSymbolizer(upid);
    if (!upid_symbolizer_status.ok()) {
      VLOG(1) << absl::Substitute("Failed to create Symbolizer function for $0 [error=$1]",
                                  upid.pid, upid_symbolizer_status.ToString());
      return profiler::SymbolizerFn(&(EmptySymbolizerFn));
    }

    upid_symbolizer = upid_symbolizer_status.ConsumeValueOrDie();
  }

  return std::bind(&ElfReader::Symbolizer::Lookup, upid_symbolizer.get(), std::placeholders::_1);
}

}  // namespace stirling
}  // namespace px
