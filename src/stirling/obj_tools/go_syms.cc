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
constexpr std::string_view kGoBuildVersionStrSymbol = "runtime.buildVersion.str";

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

// TODO(ddelnano): Between Go 1.20.2 and 1.20.4 our build version detection started failing.
// Our version detection's assumptions is very different from how Go does this internally
// and needs a signficant rehaul. That is being tracked in
// https://github.com/pixie-io/pixie/issues/1318 but in the meantime optimisitcally read the
// runtime.buildVersion.str before following our previous heuristic.
StatusOr<std::string> ReadBuildVersionDirect(ElfReader* elf_reader) {
  auto str_symbol_status = elf_reader->SearchTheOnlySymbol(kGoBuildVersionStrSymbol);
  if (!str_symbol_status.ok()) {
    return error::NotFound("Unable to find runtime.buildVersion.str");
  }
  auto str_symbol = str_symbol_status.ValueOrDie();
  PX_ASSIGN_OR_RETURN(auto symbol_bytecode, elf_reader->SymbolByteCode(".rodata", str_symbol));
  return std::string(reinterpret_cast<const char*>(symbol_bytecode.data()),
                     symbol_bytecode.size() - 1);
}

StatusOr<std::string> ReadBuildVersion(ElfReader* elf_reader) {
  auto direct_version_str = ReadBuildVersionDirect(elf_reader);
  if (!direct_version_str.ok()) {
    LOG(INFO) << absl::Substitute(
        "Falling back to the runtime.buildVersion symbol for go version detection");
  } else {
    return direct_version_str;
  }
  PX_ASSIGN_OR_RETURN(ElfReader::SymbolInfo symbol,
                      elf_reader->SearchTheOnlySymbol(kGoBuildVersionSymbol));

  // The address of this symbol points to a Golang string object.
  // But the size is for the symbol table entry, not this string object.
  symbol.size = sizeof(gostring);
  PX_ASSIGN_OR_RETURN(utils::u8string version_code, elf_reader->SymbolByteCode(".data", symbol));

  // We can't guarantee the alignment on version_string so we make a copy into an aligned address.
  gostring version_string;
  std::memcpy(&version_string, version_code.data(), sizeof(version_string));

  ElfReader::SymbolInfo version_symbol;
  version_symbol.address = reinterpret_cast<uint64_t>(version_string.ptr);
  version_symbol.size = version_string.len;

  PX_ASSIGN_OR_RETURN(utils::u8string str, elf_reader->SymbolByteCode(".data", version_symbol));
  return std::string(reinterpret_cast<const char*>(str.data()), str.size());
}

StatusOr<absl::flat_hash_map<std::string, std::vector<IntfImplTypeInfo>>> ExtractGolangInterfaces(
    ElfReader* elf_reader) {
  absl::flat_hash_map<std::string, std::vector<IntfImplTypeInfo>> interface_types;

  // All itable objects in the symbols are prefixed with this string.
  const std::string_view kITablePrefix("go.itab.");

  PX_ASSIGN_OR_RETURN(std::vector<ElfReader::SymbolInfo> itable_symbols,
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

void PrintTo(const std::vector<IntfImplTypeInfo>& infos, std::ostream* os) {
  *os << "[";
  for (auto& info : infos) {
    *os << info.ToString() << ", ";
  }
  *os << "]";
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
