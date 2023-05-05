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

#include "src/stirling/obj_tools/elf_reader.h"

#include <llvm-c/Disassembler.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/Support/TargetSelect.h>

#include <absl/container/flat_hash_set.h>
#include <set>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/utils.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/obj_tools/init.h"

namespace px {
namespace stirling {
namespace obj_tools {

// See http://elfio.sourceforge.net/elfio.pdf for examples of how to use ELFIO.

namespace {
struct LowercaseHex {
  static inline constexpr std::string_view kCharFormat = "%02x";
  static inline constexpr int kSizePerByte = 2;
  static inline constexpr bool kKeepPrintableChars = false;
};
}  // namespace

Status ElfReader::LocateDebugSymbols(const std::filesystem::path& debug_file_dir) {
  std::string build_id;
  std::string debug_link;
  bool found_symtab = false;

  // Scan all sections to find the symbol table (SHT_SYMTAB), or links to debug symbols.
  ELFIO::Elf_Half sec_num = elf_reader_.sections.size();
  for (int i = 0; i < sec_num; ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_type() == ELFIO::SHT_SYMTAB) {
      found_symtab = true;
    }

    // There are two ways to specify a debug link:
    //  1) build-id
    //  2) debuglink
    // For more details: https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html

    // Method 1: build-id.
    if (psec->get_name() == ".note.gnu.build-id") {
      // Structure of this section:
      //    namesz :   32-bit, size of "name" field
      //    descsz :   32-bit, size of "desc" field
      //    type   :   32-bit, vendor specific "type"
      //    name   :   "namesz" bytes, null-terminated string
      //    desc   :   "descsz" bytes, binary data
      int32_t name_size =
          utils::LEndianBytesToInt<int32_t>(std::string_view(psec->get_data(), sizeof(int32_t)));
      int32_t desc_size = utils::LEndianBytesToInt<int32_t>(
          std::string_view(psec->get_data() + sizeof(int32_t), sizeof(int32_t)));

      int32_t desc_pos = 3 * sizeof(int32_t) + name_size;
      std::string_view desc = std::string_view(psec->get_data() + desc_pos, desc_size);

      build_id = BytesToString<LowercaseHex>(desc);
      VLOG(1) << absl::Substitute("Found build-id: $0", build_id);
    }

    // Method 2: .gnu_debuglink.
    if (psec->get_name() == ".gnu_debuglink") {
      constexpr int kCRCBytes = 4;
      debug_link = std::string(psec->get_data(), psec->get_size() - kCRCBytes);
      VLOG(1) << absl::Substitute("Found debuglink: $0", debug_link);
    }
  }

  // In priority order, we try:
  //  1) Accessing included symtab section.
  //  2) Finding debug symbols via build-id.
  //  3) Finding debug symbols via debug_link (not yet supported).
  //
  // Example (when symbol table is not included):
  //  (1) /usr/lib/debug/.build-id/ab/cdef1234.debug
  //  (2) /usr/bin/ls.debug
  //  (2) /usr/bin/.debug/ls.debug
  //  (2) /usr/lib/debug/usr/bin/ls.debug.

  if (found_symtab) {
    debug_symbols_path_ = binary_path_;
    return Status::OK();
  }

  // Try using build-id first.
  if (!build_id.empty()) {
    std::filesystem::path symbols_file;
    std::string loc =
        absl::Substitute(".build-id/$0/$1.debug", build_id.substr(0, 2), build_id.substr(2));
    symbols_file = debug_file_dir / loc;
    VLOG(1) << absl::Substitute("Checking for debug symbols at $0", symbols_file.string());
    if (fs::Exists(symbols_file)) {
      debug_symbols_path_ = symbols_file;
      return Status::OK();
    }
  }

  // Next try using debug-link.
  if (!debug_link.empty()) {
    std::filesystem::path debug_link_path(debug_link);
    PX_ASSIGN_OR_RETURN(std::filesystem::path binary_path, fs::Canonical(binary_path_));
    std::filesystem::path binary_path_parent = binary_path.parent_path();

    std::filesystem::path candidate1 = fs::JoinPath({&binary_path_parent, &debug_link_path});
    VLOG(1) << absl::Substitute("Checking for debug symbols at $0", candidate1.string());
    if (fs::Exists(candidate1)) {
      // Ignore the candidate if it just maps back to the original path.
      bool invalid = fs::Equivalent(candidate1, binary_path).ConsumeValueOr(true);
      if (!invalid) {
        debug_symbols_path_ = candidate1;
        return Status::OK();
      }
    }

    std::filesystem::path dot_debug(".debug");
    std::filesystem::path candidate2 =
        fs::JoinPath({&binary_path_parent, &dot_debug, &debug_link_path});
    VLOG(1) << absl::Substitute("Checking for debug symbols at $0", candidate2.string());
    if (fs::Exists(candidate2)) {
      debug_symbols_path_ = candidate2;
      return Status::OK();
    }

    std::filesystem::path candidate3 = fs::JoinPath({&debug_file_dir, &binary_path});
    VLOG(1) << absl::Substitute("Checking for debug symbols at $0", candidate3.string());
    if (fs::Exists(candidate3)) {
      debug_symbols_path_ = candidate3;
      return Status::OK();
    }
  }

  return error::Internal("Could not find debug symbols for $0", binary_path_);
}

// TODO(oazizi): Consider changing binary_path to std::filesystem::path.
StatusOr<std::unique_ptr<ElfReader>> ElfReader::Create(
    const std::string& binary_path, const std::filesystem::path& debug_file_dir) {
  VLOG(1) << absl::Substitute("Creating ElfReader, [binary=$0] [debug_file_dir=$1]", binary_path,
                              debug_file_dir.string());
  auto elf_reader = std::unique_ptr<ElfReader>(new ElfReader);

  elf_reader->binary_path_ = binary_path;

  if (!elf_reader->elf_reader_.load_header_and_sections(binary_path)) {
    return error::Internal("Can't find or process ELF file $0", binary_path);
  }

  // Check for external debug symbols.
  Status s = elf_reader->LocateDebugSymbols(debug_file_dir);
  if (s.ok()) {
    std::string debug_symbols_path = elf_reader->debug_symbols_path_.string();

    bool internal_debug_symbols =
        fs::Equivalent(elf_reader->debug_symbols_path_, binary_path).ConsumeValueOr(true);

    // If external debug symbols were found, load that ELF info instead.
    if (!internal_debug_symbols) {
      std::string debug_symbols_path = elf_reader->debug_symbols_path_.string();
      LOG(INFO) << absl::Substitute("Found debug symbols file $0 for binary $1", debug_symbols_path,
                                    binary_path);
      elf_reader->elf_reader_.load_header_and_sections(debug_symbols_path);
      return elf_reader;
    }
  }

  // Debug symbols were either in the binary, or no debug symbols were found,
  // so return original elf_reader.
  return elf_reader;
}

StatusOr<ELFIO::section*> ElfReader::SymtabSection() {
  ELFIO::section* symtab_section = nullptr;
  for (int i = 0; i < elf_reader_.sections.size(); ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_type() == ELFIO::SHT_SYMTAB) {
      symtab_section = psec;
      break;
    }
    if (psec->get_type() == ELFIO::SHT_DYNSYM) {
      symtab_section = psec;
      // Dynsym is a fall-back, so don't break. We might still find the symtab section.
    }
  }
  if (symtab_section == nullptr) {
    return error::NotFound("Could not find symtab section in binary=$0", binary_path_);
  }

  return symtab_section;
}

// TODO(ddelnano): This function only works with sections that exist in LOAD segments.
// This function should be able to handle any section, but for the time being its is limited
// in scope.
StatusOr<int32_t> ElfReader::FindSegmentOffsetOfSection(std::string_view section_name) {
  PX_ASSIGN_OR_RETURN(ELFIO::section * text_section, SectionWithName(section_name));
  auto section_offset = text_section->get_offset();

  for (int i = 0; i < elf_reader_.segments.size() - 1; ++i) {
    ELFIO::segment* current_segment = elf_reader_.segments[i];
    ELFIO::segment* next_segment = elf_reader_.segments[i + 1];

    auto expected_type = ELFIO::PT_LOAD;
    if (current_segment->get_type() != expected_type || next_segment->get_type() != expected_type)
      continue;

    auto current_segment_offset = current_segment->get_offset();
    auto next_segment_offset = next_segment->get_offset();

    // Check to see if the section we are searching for exists
    // between the two contiguous segments we are looping through.
    if (next_segment_offset >= section_offset && section_offset >= current_segment_offset) {
      return current_segment_offset;
    }
  }
  return error::NotFound("Could not find segment offset of section '$0'", section_name);
}

static auto NoTextStartAddrError =
    Status(px::statuspb::INVALID_ARGUMENT,
           "Must provide text_start_addr to ELFReader to use Symbol resolution functions");

StatusOr<std::vector<ElfReader::SymbolInfo>> ElfReader::SearchSymbols(
    std::string_view search_symbol, SymbolMatchType match_type, std::optional<int> symbol_type,
    bool stop_at_first_match) {
  PX_ASSIGN_OR_RETURN(ELFIO::section * symtab_section, SymtabSection());

  std::vector<SymbolInfo> symbol_infos;

  // Scan all symbols inside the symbol table.
  const ELFIO::symbol_section_accessor symbols(elf_reader_, symtab_section);
  for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
    std::string name;
    ELFIO::Elf64_Addr addr = 0;
    ELFIO::Elf_Xword size = 0;
    unsigned char bind = 0;
    unsigned char type = ELFIO::STT_NOTYPE;
    ELFIO::Elf_Half section_index;
    unsigned char other;
    symbols.get_symbol(j, name, addr, size, bind, type, section_index, other);

    if (symbol_type.has_value() && type != symbol_type.value()) {
      continue;
    }

    if (!MatchesSymbol(name, {match_type, search_symbol})) {
      continue;
    }

    symbol_infos.push_back({std::move(name), type, addr, size});

    if (stop_at_first_match) {
      break;
    }
  }
  return symbol_infos;
}

StatusOr<std::vector<ElfReader::SymbolInfo>> ElfReader::ListFuncSymbols(
    std::string_view search_symbol, SymbolMatchType match_type) {
  PX_ASSIGN_OR_RETURN(std::vector<ElfReader::SymbolInfo> symbol_infos,
                      SearchSymbols(search_symbol, match_type, /*symbol_type*/ ELFIO::STT_FUNC));

  absl::flat_hash_set<uint64_t> symbol_addrs;
  for (auto& symbol_info : symbol_infos) {
    // Symbol address has already been seen.
    // Note that multiple symbols can point to the same address.
    // But symbol names cannot be duplicate.
    if (!symbol_addrs.insert(symbol_info.address).second) {
      VLOG(1) << absl::Substitute(
          "Found multiple symbols to the same address ($0). New behavior does not filter these "
          "out. Symbol=$1",
          symbol_info.address, symbol_info.name);
    }
  }

  return symbol_infos;
}

StatusOr<ElfReader::SymbolInfo> ElfReader::SearchTheOnlySymbol(std::string_view symbol) {
  PX_ASSIGN_OR_RETURN(std::vector<ElfReader::SymbolInfo> symbol_infos,
                      SearchSymbols(symbol, SymbolMatchType::kExact, /*symbol_type*/ std::nullopt,
                                    /*stop_at_first_match*/ true));
  if (symbol_infos.empty()) {
    return error::NotFound("Symbol $0 not found", symbol);
  }
  return std::move(symbol_infos[0]);
}

std::optional<int64_t> ElfReader::SymbolAddress(std::string_view symbol) {
  auto symbol_infos_or = SearchSymbols(symbol, SymbolMatchType::kExact);
  if (symbol_infos_or.ok()) {
    const auto& symbol_infos = symbol_infos_or.ValueOrDie();
    if (!symbol_infos.empty()) {
      DCHECK_EQ(symbol_infos.size(), 1U);
      return symbol_infos.front().address;
    }
  }
  return std::nullopt;
}

StatusOr<std::optional<std::string>> ElfReader::AddrToSymbol(size_t sym_addr) {
  PX_ASSIGN_OR_RETURN(ELFIO::section * symtab_section, SymtabSection());

  const ELFIO::symbol_section_accessor symbols(elf_reader_, symtab_section);

  // Call ELFIO to get symbol by address.
  // ELFIO looks up the symbol and then populates name, size, type, etc.
  // We only care about the name, but need to declare the other variables as well.
  const ELFIO::Elf64_Addr addr = sym_addr;
  std::string name;
  ELFIO::Elf_Xword size = 0;
  unsigned char bind = 0;
  unsigned char type = ELFIO::STT_NOTYPE;
  ELFIO::Elf_Half section_index;
  unsigned char other;
  bool found = symbols.get_symbol(addr, name, size, bind, type, section_index, other);

  if (!found) {
    return std::optional<std::string>();
  }

  return std::optional<std::string>(std::move(name));
}

// TODO(oazizi): Optimize by indexing or switching to binary search if we can guarantee addresses
//               are ordered.
StatusOr<std::optional<std::string>> ElfReader::InstrAddrToSymbol(size_t sym_addr) {
  PX_ASSIGN_OR_RETURN(ELFIO::section * symtab_section, SymtabSection());

  const ELFIO::symbol_section_accessor symbols(elf_reader_, symtab_section);
  for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
    // Call ELFIO to get symbol by index.
    // ELFIO looks up the index and then populates name, addr, size, type, etc.
    // We only care about the name and addr, but need to declare the other variables as well.
    std::string name;
    ELFIO::Elf64_Addr addr = 0;
    ELFIO::Elf_Xword size = 0;
    unsigned char bind = 0;
    unsigned char type = ELFIO::STT_NOTYPE;
    ELFIO::Elf_Half section_index;
    unsigned char other;
    symbols.get_symbol(j, name, addr, size, bind, type, section_index, other);

    if (sym_addr >= addr && sym_addr < addr + size) {
      return std::optional<std::string>(llvm::demangle(name));
    }
  }

  return std::optional<std::string>();
}

StatusOr<std::unique_ptr<ElfReader::Symbolizer>> ElfReader::GetSymbolizer() {
  PX_ASSIGN_OR_RETURN(ELFIO::section * symtab_section, SymtabSection());

  auto symbolizer = std::make_unique<ElfReader::Symbolizer>();

  const ELFIO::symbol_section_accessor symbols(elf_reader_, symtab_section);
  for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
    // Call ELFIO to get symbol by index.
    // ELFIO looks up the index and then populates name, addr, size, type, etc.
    // We only care about the name and addr, but need to declare the other variables as well.
    std::string name;
    ELFIO::Elf64_Addr addr = 0;
    ELFIO::Elf_Xword size = 0;
    unsigned char bind = 0;
    unsigned char type = ELFIO::STT_NOTYPE;
    ELFIO::Elf_Half section_index;
    unsigned char other;
    symbols.get_symbol(j, name, addr, size, bind, type, section_index, other);

    if (type == ELFIO::STT_FUNC) {
      symbolizer->AddEntry(addr, size, llvm::demangle(name));
    }
  }

  return symbolizer;
}

void ElfReader::Symbolizer::AddEntry(size_t addr, size_t size, std::string name) {
  symbols_.emplace(addr, SymbolAddrInfo{size, std::move(name)});
}

std::string_view ElfReader::Symbolizer::Lookup(size_t addr) const {
  static std::string symbol_str;

  // Find the first symbol for which the address_range_start > addr.
  auto iter = symbols_.upper_bound(addr);

  if (iter == symbols_.begin() || symbols_.empty()) {
    symbol_str = absl::StrFormat("0x%016llx", addr);
    return symbol_str;
  }

  // std::upper_bound will make us overshoot our potential match,
  // so go back by one, and check if it is indeed a match.
  --iter;
  if (addr >= iter->first && addr < iter->first + iter->second.size) {
    return iter->second.name;
  }

  // Couldn't find the address.
  symbol_str = absl::StrFormat("0x%016llx", addr);
  return symbol_str;
}

namespace {

enum Arch {
  ArchX86_64,
  ArchAarch64,
};
constexpr char x86_64_triple[] = "x86_64-pc-linux";
constexpr char aarch64_triple[] = "aarch64-pc-linux";

bool IsRetInstX86_64(const uint8_t* bytes, int inst_size) {
  if (inst_size < 1) {
    return false;
  }
  uint8_t opcode = *bytes;
  // https://c9x.me/x86/html/file_module_x86_id_280.html for full list.
  //
  // Near return to calling procedure.
  constexpr uint8_t kRetn = '\xc3';

  // Far return to calling procedure.
  constexpr uint8_t kRetf = '\xcb';

  // Near return to calling procedure and pop imm16 bytes from stack.
  constexpr uint8_t kRetnImm = '\xc2';

  // Far return to calling procedure and pop imm16 bytes from stack.
  constexpr uint8_t kRetfImm = '\xca';

  return opcode == kRetn || opcode == kRetf || opcode == kRetnImm || opcode == kRetfImm;
}

bool IsRetInstAarch64(const uint8_t* bytes, int inst_size) {
  // A64 has fixed width instructions.
  static constexpr int kInstrBytes = 4;
  if (inst_size != kInstrBytes) {
    return false;
  }
  uint32_t instr;
  // TODO(james): this will fail on non-little endian machines.
  // eg. if we're processing a little endian arm elf file on an x86_64 machine.
  memcpy(&instr, bytes, kInstrBytes);

  // Per the A64 docs the RET instruction encodes the branch target register in bits 5:9, so we mask
  // out those bits.
  constexpr uint32_t ret_reg_mask = ~(0b11111 << 5);
  // Per the A64 docs a RET instruction sets all these bits exactly as below (ignoring 5:9 in the
  // mask above).
  constexpr uint32_t ret_instr_bits = 0b11010110010111110000000000000000;

  return (instr & ret_reg_mask) == ret_instr_bits;
}

/**
 * RAII wrapper around LLVMDisasmContextRef.
 */
class LLVMDisasmContext {
  using IsRetInstFn = std::function<bool(const uint8_t*, int)>;

 public:
  explicit LLVMDisasmContext(Arch arch) {
    InitLLVMOnce();

    const char* triple = "";
    switch (arch) {
      case ArchX86_64:
        triple = x86_64_triple;
        is_ret_inst_impl_ = IsRetInstX86_64;
        break;
      case ArchAarch64:
        triple = aarch64_triple;
        is_ret_inst_impl_ = IsRetInstAarch64;
        break;
    }

    ref_ = LLVMCreateDisasm(triple,
                            /*DisInfo*/ nullptr, /*TagType*/ 0, /*LLVMOpInfoCallback*/ nullptr,
                            /*LLVMSymbolLookupCallback*/ nullptr);
  }

  bool IsRetInst(const uint8_t* bytes, int instr_size) {
    return is_ret_inst_impl_(bytes, instr_size);
  }

  ~LLVMDisasmContext() { LLVMDisasmDispose(ref_); }

  LLVMDisasmContextRef ref() const { return ref_; }

 private:
  LLVMDisasmContextRef ref_ = nullptr;
  IsRetInstFn is_ret_inst_impl_;
};

std::unique_ptr<absl::flat_hash_map<Arch, std::unique_ptr<LLVMDisasmContext>>> g_disasm_registry;

LLVMDisasmContext* GetLLVMDisasmContextForArch(Arch arch) {
  if (g_disasm_registry == nullptr) {
    g_disasm_registry =
        std::make_unique<absl::flat_hash_map<Arch, std::unique_ptr<LLVMDisasmContext>>>();
  }
  if (!g_disasm_registry->contains(arch)) {
    (*g_disasm_registry)[arch] = std::make_unique<LLVMDisasmContext>(arch);
  }
  return (*g_disasm_registry)[arch].get();
}

std::vector<uint64_t> FindRetInsts(Arch arch, utils::u8string_view byte_code) {
  if (byte_code.empty()) {
    return {};
  }

  auto disasm = GetLLVMDisasmContextForArch(arch);

  // Size of the buffer to hold disassembled assembly code. Since we do not really use the assembly
  // code, we just provide a small buffer.
  // (Unfortunately, nullptr and 0 crashes.)
  constexpr int kBufSize = 32;
  // Initialize array to zero.
  char buf[kBufSize] = {};

  uint64_t pc = 0;
  auto* codes = const_cast<uint8_t*>(byte_code.data());
  size_t codes_size = byte_code.size();
  int inst_size = 0;

  std::vector<uint64_t> res;
  do {
    if (disasm->IsRetInst(codes, inst_size)) {
      res.push_back(pc);
    }
    // TODO(yzhao): MCDisassembler::getInst() works better here, because it returns a MCInst, with
    // an opcode for examination. Unfortunately, MCDisassembler is difficult to create without
    // class LLVMDisasmContex, which is not exposed.
    inst_size = LLVMDisasmInstruction(disasm->ref(), codes, codes_size, pc, buf, kBufSize);

    pc += inst_size;
    codes += inst_size;
    codes_size -= inst_size;
  } while (inst_size != 0);
  return res;
}

StatusOr<Arch> GetArchFromELFMachine(ELFIO::Elf_Half machine) {
  switch (machine) {
    case ELFIO::EM_X86_64:
      return ArchX86_64;
    case ELFIO::EM_AARCH64:
      return ArchAarch64;
    default:
      return Status(statuspb::INVALID_ARGUMENT,
                    absl::Substitute("ELF file uses unsupported architecture: $0", machine));
  }
}

}  // namespace

StatusOr<std::vector<uint64_t>> ElfReader::FuncRetInstAddrs(const SymbolInfo& func_symbol) {
  constexpr std::string_view kDotText = ".text";
  PX_ASSIGN_OR_RETURN(utils::u8string byte_code, SymbolByteCode(kDotText, func_symbol));
  PX_ASSIGN_OR_RETURN(auto arch, GetArchFromELFMachine(elf_reader_.get_machine()));
  std::vector<uint64_t> addrs = FindRetInsts(arch, byte_code);
  for (auto& offset : addrs) {
    offset += func_symbol.address;
  }
  return addrs;
}

StatusOr<ELFIO::section*> ElfReader::SectionWithName(std::string_view section_name) {
  for (int i = 0; i < elf_reader_.sections.size(); ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_name() == section_name) {
      return psec;
    }
  }
  return error::NotFound("Could not find section=$0 in binary=$1", section_name, binary_path_);
}

StatusOr<utils::u8string> ElfReader::SymbolByteCode(std::string_view section,
                                                    const SymbolInfo& symbol) {
  PX_ASSIGN_OR_RETURN(ELFIO::section * text_section, SectionWithName(section));
  int offset = symbol.address - text_section->get_address() + text_section->get_offset();

  std::ifstream ifs(binary_path_, std::ios::binary);
  if (!ifs.seekg(offset)) {
    return error::Internal("Failed to seek position=$0 in binary=$1", offset, binary_path_);
  }
  // To protect against our ELF parsing logic locating bogus memory, set a bound on
  // how large of a string we will allocate. SymbolByteCode's main use case is to determine
  // return instructions for the crypto/tls.(*Conn).Write and crypto/tls.(*Conn).Read Go functions.
  // These symbols are roughly 2 KiB and were used to inform the threshold below. We apply
  // an additional 100x multiplier for additional headroom.
  // See https://github.com/pixie-io/pixie/issues/1111 for more details.
  if (symbol.size > 100 * 2048) {
    return error::Internal(
        "ELF symbol=$0 bytecode detected as size=$1 bytes. Refusing to preallocate that much "
        "memory",
        symbol.name, symbol.size);
  }
  utils::u8string byte_code(symbol.size, '\0');
  auto* buf = reinterpret_cast<char*>(byte_code.data());
  if (!ifs.read(buf, symbol.size)) {
    return error::Internal("Failed to read size=$0 bytes from offset=$1 in binary=$2", symbol.size,
                           offset, binary_path_);
  }
  if (ifs.gcount() != static_cast<int64_t>(symbol.size)) {
    return error::Internal("Only read size=$0 bytes from offset=$1 in binary=$2, expect $3 bytes",
                           symbol.size, offset, binary_path_, ifs.gcount());
  }
  return byte_code;
}

StatusOr<uint64_t> ElfReader::GetVirtualAddrAtOffsetZero() {
  const ELFIO::segment* first_loadable_segment = nullptr;
  for (int i = 0; i < elf_reader_.segments.size(); i++) {
    ELFIO::segment* segment = elf_reader_.segments[i];
    if (segment->get_type() == ELFIO::PT_LOAD) {
      first_loadable_segment = segment;
      break;
    }
  }

  if (first_loadable_segment == nullptr) {
    return Status(statuspb::INTERNAL, "No loadable segments in ELF file");
  }
  uint64_t virt_addr = first_loadable_segment->get_virtual_address();
  uint64_t offset = first_loadable_segment->get_offset();
  return virt_addr - offset;
}

ELFIO::Elf_Half ElfReader::ELFType() { return elf_reader_.get_type(); }

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
