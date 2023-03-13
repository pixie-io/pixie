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
#include <utility>

#include <absl/functional/bind_front.h>

#include "src/common/base/base.h"
#include "src/common/system/proc_pid_path.h"
#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/elf_symbolizer.h"
#include "src/stirling/utils/proc_path_tools.h"

using ::px::stirling::obj_tools::ElfReader;
using ::px::system::ProcPidRootPath;

namespace px {
namespace stirling {

StatusOr<std::unique_ptr<Symbolizer>> ElfSymbolizer::Create() {
  ElfSymbolizer* elf_symbolizer = new ElfSymbolizer();
  auto symbolizer = std::unique_ptr<Symbolizer>(elf_symbolizer);
  return symbolizer;
}

void ElfSymbolizer::DeleteUPID(const struct upid_t& upid) { symbolizers_.erase(upid); }

StatusOr<std::unique_ptr<ElfSymbolizer::SymbolizerWithConverter>> CreateUPIDSymbolizer(
    const struct upid_t& upid) {
  const pid_t pid = upid.pid;
  const system::ProcParser proc_parser;
  PX_ASSIGN_OR_RETURN(const auto proc_exe, proc_parser.GetExePath(pid));
  PX_ASSIGN_OR_RETURN(auto elf_reader, ElfReader::Create(ProcPidRootPath(pid, proc_exe.string())));

  PX_ASSIGN_OR_RETURN(auto symbolizer, elf_reader->GetSymbolizer());
  PX_ASSIGN_OR_RETURN(auto converter,
                      obj_tools::ElfAddressConverter::Create(elf_reader.get(), pid));
  return std::make_unique<ElfSymbolizer::SymbolizerWithConverter>(std::move(symbolizer),
                                                                  std::move(converter));
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

  std::unique_ptr<SymbolizerWithConverter>& symbolizer_with_converter = symbolizers_[upid];
  if (symbolizer_with_converter == nullptr) {
    auto upid_symbolizer_status = CreateUPIDSymbolizer(upid);
    if (!upid_symbolizer_status.ok()) {
      VLOG(1) << absl::Substitute("Failed to create Symbolizer function for $0 [error=$1]",
                                  upid.pid, upid_symbolizer_status.ToString());
      return profiler::SymbolizerFn(&(EmptySymbolizerFn));
    }
    symbolizer_with_converter = upid_symbolizer_status.ConsumeValueOrDie();
  }

  return absl::bind_front(&ElfSymbolizer::SymbolizerWithConverter::Lookup,
                          symbolizer_with_converter.get());
}

std::string_view ElfSymbolizer::SymbolizerWithConverter::Lookup(uint64_t virtual_addr) const {
  auto binary_addr = converter_->VirtualAddrToBinaryAddr(virtual_addr);
  return symbolizer_->Lookup(binary_addr);
}

}  // namespace stirling
}  // namespace px
