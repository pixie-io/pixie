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
#include "src/stirling/utils/binary_decoder.h"

#include <utility>

namespace px {
namespace stirling {
namespace obj_tools {

using px::utils::u8string_view;
using read_ptr_func_t = std::function<uint64_t(u8string_view)>;

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

constexpr std::string_view kGoBuildInfoSection = ".go.buildinfo";
// kGoBuildInfoMagic corresponds to "\xff Go buildinf:"
// https://github.com/golang/go/blob/1dbbafc70fd3e2c284469ab3e0936c1bb56129f6/src/debug/buildinfo/buildinfo.go#L49
std::string_view kGoBuildInfoMagic =
    CreateStringView<char>("\xff\x20\x47\x6f\x20\x62\x75\x69\x6c\x64\x69\x6e\x66\x3a");

// Reads a Go string encoded within a buildinfo header. This function is meant to provide the same
// functionality as
// https://github.com/golang/go/blob/master/src/debug/buildinfo/buildinfo.go#L244C37-L244C44
StatusOr<std::string> ReadGoString(ElfReader* elf_reader, uint64_t ptr_size, uint64_t ptr_addr,
                                   read_ptr_func_t read_ptr) {
  PX_ASSIGN_OR_RETURN(u8string_view data_addr, elf_reader->BinaryByteCode(ptr_addr, ptr_size));
  PX_ASSIGN_OR_RETURN(u8string_view data_len,
                      elf_reader->BinaryByteCode(ptr_addr + ptr_size, ptr_size));

  PX_ASSIGN_OR_RETURN(uint64_t vaddr_offset, elf_reader->GetVirtualAddrAtOffsetZero());
  ptr_addr = read_ptr(data_addr) - vaddr_offset;
  uint64_t str_length = read_ptr(data_len);

  PX_ASSIGN_OR_RETURN(std::string_view go_version_bytecode,
                      elf_reader->BinaryByteCode<char>(ptr_addr, str_length));
  return std::string(go_version_bytecode);
}

// Reads the buildinfo header embedded in the .go.buildinfo ELF section in order to determine the go
// toolchain version. This function emulates what the go version cli performs as seen
// https://github.com/golang/go/blob/cb7a091d729eab75ccfdaeba5a0605f05addf422/src/debug/buildinfo/buildinfo.go#L151-L221
StatusOr<std::string> ReadGoBuildVersion(ElfReader* elf_reader) {
  PX_ASSIGN_OR_RETURN(ELFIO::section * section, elf_reader->SectionWithName(kGoBuildInfoSection));
  int offset = section->get_offset();
  PX_ASSIGN_OR_RETURN(std::string_view buildInfoByteCode,
                      elf_reader->BinaryByteCode<char>(offset, 64 * 1024));

  BinaryDecoder binary_decoder(buildInfoByteCode);

  PX_CHECK_OK(binary_decoder.ExtractStringUntil(kGoBuildInfoMagic));
  PX_ASSIGN_OR_RETURN(uint8_t ptr_size, binary_decoder.ExtractBEInt<uint8_t>());
  PX_ASSIGN_OR_RETURN(uint8_t endianness, binary_decoder.ExtractBEInt<uint8_t>());

  // If the endianness has its second bit set, then the go version immediately follows the 32 bit
  // header specified by the varint encoded string data
  if ((endianness & 0x2) != 0) {
    // Skip the remaining 16 bytes of buildinfo header
    PX_CHECK_OK(binary_decoder.ExtractBufIgnore(16));

    PX_ASSIGN_OR_RETURN(uint64_t size, binary_decoder.ExtractUVarInt());
    PX_ASSIGN_OR_RETURN(std::string_view go_version, binary_decoder.ExtractString(size));
    return std::string(go_version);
  }

  read_ptr_func_t read_ptr;
  switch (endianness) {
    case 0x0: {
      if (ptr_size == 4) {
        read_ptr = [&](u8string_view str_view) {
          return utils::LEndianBytesToInt<uint32_t, 4>(str_view);
        };
      } else if (ptr_size == 8) {
        read_ptr = [&](u8string_view str_view) {
          return utils::LEndianBytesToInt<uint64_t, 8>(str_view);
        };
      } else {
        return error::NotFound(absl::Substitute(
            "Binary reported pointer size=$0, refusing to parse non go binary", ptr_size));
      }
      break;
    }
    case 0x1:
      if (ptr_size == 4) {
        read_ptr = [&](u8string_view str_view) {
          return utils::BEndianBytesToInt<uint64_t, 4>(str_view);
        };
      } else if (ptr_size == 8) {
        read_ptr = [&](u8string_view str_view) {
          return utils::BEndianBytesToInt<uint64_t, 8>(str_view);
        };
      } else {
        return error::NotFound(absl::Substitute(
            "Binary reported pointer size=$0, refusing to parse non go binary", ptr_size));
      }
      break;
    default: {
      auto msg =
          absl::Substitute("Invalid endianness=$0, refusing to parse non go binary", endianness);
      DCHECK(false) << msg;
      return error::NotFound(msg);
    }
  }

  PX_ASSIGN_OR_RETURN(uint64_t vaddr_offset, elf_reader->GetVirtualAddrAtOffsetZero());

  PX_ASSIGN_OR_RETURN(auto s, binary_decoder.ExtractString<u8string_view::value_type>(ptr_size));
  uint64_t ptr_addr = read_ptr(s) - vaddr_offset;

  return ReadGoString(elf_reader, ptr_size, ptr_addr, read_ptr);
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
