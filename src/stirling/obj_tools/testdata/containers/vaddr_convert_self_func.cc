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

#include "src/stirling/obj_tools/address_converter.h"
#include "src/stirling/utils/proc_path_tools.h"

// Using extern C to avoid name mangling since ElfReader must be able to address this
// by its symbol name.
extern "C" {
void TestFunc() {}

}  // extern "C"

using px::stirling::GetSelfPath;
using px::stirling::obj_tools::ElfAddressConverter;
using px::stirling::obj_tools::ElfReader;
using px::stirling::obj_tools::SymbolMatchType;

// This utility performs an assertion that the ElfAddressConverter::VirtualAddrToBinaryAddr function
// returns the correct (consistent with ElfReader and `nm` cli output) binary address for a given
// function. This is used to test our address conversion logic when a PIE binary is memory mapped
// differently from the common scenario (where the process's ELF segments are mapped at the
// lowest VMA), such as when an unlimited stack size ulimit is set on a process.
int main() {
  LOG(INFO) << "Running";

  // This sleep is required otherwise when it is run inside a container (via ContainerRunner) we
  // will fail to detect the child process's pid during the test case that uses this.
  sleep(5);

  std::filesystem::path self_path = GetSelfPath().ValueOrDie();
  PX_ASSIGN_OR(auto elf_reader, ElfReader::Create(self_path.string()), return -1);
  PX_ASSIGN_OR(std::vector<ElfReader::SymbolInfo> syms,
               elf_reader->ListFuncSymbols("TestFunc", SymbolMatchType::kSubstr));

  PX_ASSIGN_OR(auto converter, ElfAddressConverter::Create(elf_reader.get(), getpid()), return -1);
  auto symbol_addr = converter->VirtualAddrToBinaryAddr(reinterpret_cast<uint64_t>(&TestFunc));

  auto expected_addr = syms[0].address;
  if (symbol_addr != expected_addr) {
    LOG(ERROR) << absl::Substitute(
        "Expected ElfAddressConverter address=$0 to match binary address=$1", symbol_addr,
        expected_addr);
    return -1;
  }
  return 0;
}
