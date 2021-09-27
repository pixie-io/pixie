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

#include "src/stirling/obj_tools/go_syms.h"

namespace px {
namespace stirling {
namespace obj_tools {

namespace {

// This is defined in src/stirling/bpf_tools/bcc_bpf_intf/go_types.h as well.
// Duplicated to avoid undesired dependency.
struct gostring {
  const char* ptr;
  int64_t len;
};

}  // namespace

StatusOr<std::string> ReadBuildVersion(ElfReader* elf_reader) {
  PL_ASSIGN_OR_RETURN(ElfReader::SymbolInfo symbol,
                      elf_reader->SearchTheOnlySymbol(kGoBuildVersionSymbol));

  // The address of this symbol points to a Golang string object.
  // But the size is for the symbol table entry, not this string object.
  symbol.size = sizeof(gostring);
  PL_ASSIGN_OR_RETURN(utils::u8string version_code, elf_reader->SymbolByteCode(".data", symbol));

  const auto* version_string = reinterpret_cast<const gostring*>(version_code.data());

  ElfReader::SymbolInfo version_symbol;
  version_symbol.address = reinterpret_cast<uint64_t>(version_string->ptr);
  version_symbol.size = version_string->len;

  PL_ASSIGN_OR_RETURN(utils::u8string str, elf_reader->SymbolByteCode(".data", version_symbol));
  return std::string(reinterpret_cast<const char*>(str.data()), str.size());
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
