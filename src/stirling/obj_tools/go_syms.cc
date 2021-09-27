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

#include <utility>

namespace px {
namespace stirling {
namespace obj_tools {

// This symbol points to a static string variable that describes the Golang tool-chain version used
// to build the executable. This symbol is embedded in a Golang executable's data section.
constexpr std::string_view kGoBuildVersionSymbol = "runtime.buildVersion";

namespace {

// This is defined in src/stirling/bpf_tools/bcc_bpf_intf/go_types.h as well.
// Duplicated to avoid undesired dependency.
struct gostring {
  const char* ptr;
  int64_t len;
};

}  // namespace

bool IsGoExecutable(ElfReader* elf_reader) {
  return elf_reader->SearchTheOnlySymbol(obj_tools::kGoBuildVersionSymbol).ok();
}

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

StatusOr<absl::flat_hash_map<std::string, std::vector<IntfImplTypeInfo>>> ExtractGolangInterfaces(
    ElfReader* elf_reader) {
  absl::flat_hash_map<std::string, std::vector<IntfImplTypeInfo>> interface_types;

  // All itable objects in the symbols are prefixed with this string.
  const std::string_view kITablePrefix("go.itab.");

  PL_ASSIGN_OR_RETURN(std::vector<ElfReader::SymbolInfo> itable_symbols,
                      elf_reader->SearchSymbols(kITablePrefix, SymbolMatchType::kPrefix,
                                                /*symbol_type*/ ELFIO::STT_OBJECT));

  for (const auto& sym : itable_symbols) {
    // Expected format is:
    //  go.itab.<type_name>,<interface_name>
    std::vector<std::string_view> sym_split = absl::StrSplit(sym.name, ",");
    if (sym_split.size() != 2) {
      LOG(WARNING) << absl::Substitute("Ignoring unexpected itable format: $0", sym.name);
      continue;
    }

    std::string_view interface_name = sym_split[1];
    std::string_view type = sym_split[0];
    type.remove_prefix(kITablePrefix.size());

    IntfImplTypeInfo info;

    info.type_name = type;
    info.address = sym.address;

    interface_types[std::string(interface_name)].push_back(std::move(info));
  }

  return interface_types;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
