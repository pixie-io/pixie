#include "src/stirling/obj_tools/elf_tools.h"

#include <llvm-c/Disassembler.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/Support/TargetSelect.h>

#include <absl/container/flat_hash_set.h>
#include <set>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/utils.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/obj_tools/init.h"

namespace pl {
namespace stirling {
namespace elf_tools {

// See http://elfio.sourceforge.net/elfio.pdf for examples of how to use ELFIO.

namespace {
struct LowercaseHex {
  static inline constexpr std::string_view kCharFormat = "%02x";
  static inline constexpr int kSizePerByte = 2;
  static inline constexpr bool kKeepPrintableChars = false;
};
}  // namespace

StatusOr<std::unique_ptr<ElfReader>> ElfReader::Create(const std::string& binary_path,
                                                       std::string_view debug_file_dir) {
  VLOG(1) << absl::Substitute("Creating ElfReader, [binary=$0] [debug_file_dir=$1]", binary_path,
                              debug_file_dir);
  auto elf_reader = std::unique_ptr<ElfReader>(new ElfReader);

  elf_reader->binary_path_ = binary_path;

  if (!elf_reader->elf_reader_.load(binary_path, /* skip_segments */ true)) {
    return error::Internal("Can't find or process ELF file $0", binary_path);
  }

  std::string build_id;
  std::string debug_link;
  bool found_symtab = false;

  // Scan all sections to find the symbol table (SHT_SYMTAB), or links to debug symbols.
  ELFIO::Elf_Half sec_num = elf_reader->elf_reader_.sections.size();
  for (int i = 0; i < sec_num; ++i) {
    ELFIO::section* psec = elf_reader->elf_reader_.sections[i];
    if (psec->get_type() == SHT_SYMTAB) {
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
    // This is currently disabled because our modified ELFIO currently removes PROGBITS sections,
    // including .gnu_debuglink.
    // TODO(oazizi): Re-enable this section after tweaking ELFIO.
    //
    // if (psec->get_name() == ".gnu_debuglink") {
    //    constexpr int kCRCBytes = 4;
    //    debug_link = std::string(psec->get_data(), psec->get_size() - kCRCBytes);
    //    LOG(INFO) << absl::Substitute("Found debuglink: $0", debug_link);
    // }
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
    return elf_reader;
  }

  if (!build_id.empty()) {
    std::string symbols_file;
    symbols_file = absl::Substitute("$0/.build-id/$1/$2.debug", debug_file_dir,
                                    build_id.substr(0, 2), build_id.substr(2));
    if (fs::Exists(symbols_file).ok()) {
      LOG(INFO) << absl::Substitute("Found debug symbols file $0 for binary $1", symbols_file,
                                    binary_path);
      elf_reader->elf_reader_.load(symbols_file);
      return elf_reader;
    }
  }

  LOG_IF(WARNING, !debug_link.empty()) << absl::Substitute(
      "Resolving debug symbols via .gnu_debuglink is not currently supported [binary=$0].",
      binary_path);

  // Couldn't find debug symbols, so return original elf_reader.
  return elf_reader;
}

StatusOr<std::vector<ElfReader::SymbolInfo>> ElfReader::SearchSymbols(
    std::string_view search_symbol, SymbolMatchType match_type, std::optional<int> symbol_type) {
  ELFIO::section* symtab_section = nullptr;
  for (int i = 0; i < elf_reader_.sections.size(); ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_type() == SHT_SYMTAB) {
      symtab_section = psec;
      break;
    }
  }
  if (symtab_section == nullptr) {
    return error::NotFound("Could not find symtab section in binary=$0", binary_path_);
  }

  std::vector<SymbolInfo> symbol_infos;

  // Scan all symbols inside the symbol table.
  const ELFIO::symbol_section_accessor symbols(elf_reader_, symtab_section);
  for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
    std::string name;
    ELFIO::Elf64_Addr addr = 0;
    ELFIO::Elf_Xword size = 0;
    unsigned char bind = 0;
    unsigned char type = STT_NOTYPE;
    ELFIO::Elf_Half section_index;
    unsigned char other;
    symbols.get_symbol(j, name, addr, size, bind, type, section_index, other);

    if (symbol_type.has_value() && type != symbol_type.value()) {
      continue;
    }

    // Check for symbol match.
    bool match = false;
    switch (match_type) {
      case SymbolMatchType::kExact:
        match = (name == search_symbol);
        break;
      case SymbolMatchType::kPrefix:
        match = absl::StartsWith(name, search_symbol);
        break;
      case SymbolMatchType::kSuffix:
        match = absl::EndsWith(name, search_symbol);
        break;
      case SymbolMatchType::kSubstr:
        match = (name.find(search_symbol) != std::string::npos);
        break;
    }
    if (!match) {
      continue;
    }
    symbol_infos.push_back({std::move(name), type, addr, size});
  }
  return symbol_infos;
}

StatusOr<std::vector<ElfReader::SymbolInfo>> ElfReader::ListFuncSymbols(
    std::string_view search_symbol, SymbolMatchType match_type) {
  PL_ASSIGN_OR_RETURN(std::vector<ElfReader::SymbolInfo> symbol_infos,
                      SearchSymbols(search_symbol, match_type, STT_FUNC));

  absl::flat_hash_set<uint64_t> symbol_addrs;
  for (auto& symbol_info : symbol_infos) {
    // Symbol address has already been seen.
    // Note that multiple symbols can point to the same address.
    // But symbol names cannot be duplicate.
    if (symbol_addrs.insert(symbol_info.address).second) {
      LOG(WARNING)
          << "Found multiple symbols to the same address. New behavior does not filter these out.";
    }
  }

  return symbol_infos;
}

std::optional<int64_t> ElfReader::SymbolAddress(std::string_view symbol) {
  auto symbol_infos_or = SearchSymbols(symbol, SymbolMatchType::kExact);
  if (symbol_infos_or.ok()) {
    const auto& symbol_infos = symbol_infos_or.ValueOrDie();
    if (!symbol_infos.empty()) {
      DCHECK_EQ(symbol_infos.size(), 1);
      return symbol_infos.front().address;
    }
  }
  return std::nullopt;
}

namespace {

/**
 * RAII wrapper around LLVMDisasmContextRef.
 */
class LLVMDisasmContext {
 public:
  LLVMDisasmContext() {
    InitLLVMOnce();

    // TripleName is ARCHITECTURE-VENDOR-OPERATING_SYSTEM.
    // See https://llvm.org/doxygen/Triple_8h_source.html
    // TODO(yzhao): Change to get TripleName from the system, instead of hard coding.
    ref_ = LLVMCreateDisasm(/*TripleName*/ "x86_64-pc-linux",
                            /*DisInfo*/ nullptr, /*TagType*/ 0, /*LLVMOpInfoCallback*/ nullptr,
                            /*LLVMSymbolLookupCallback*/ nullptr);
  }

  ~LLVMDisasmContext() { LLVMDisasmDispose(ref_); }

  LLVMDisasmContextRef ref() const { return ref_; }

 private:
  LLVMDisasmContextRef ref_ = nullptr;
};

bool IsRetInst(uint8_t code) {
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

  return code == kRetn || code == kRetf || code == kRetnImm || code == kRetfImm;
}

std::vector<uint64_t> FindRetInsts(utils::u8string_view byte_code) {
  if (byte_code.empty()) {
    return {};
  }

  // TODO(yzhao): This is a short-term quick way to avoid unnecessary overheads.
  // We should create LLVMDisasmContext object inside SocketTraceConnector and pass it around.
  static const LLVMDisasmContext kLLVMDisasmContext;

  // Size of the buffer to hold disassembled assembly code. Since we do not really use the assembly
  // code, we just provide a small buffer.
  // (Unfortunately, nullptr and 0 crashes.)
  constexpr int kBufSize = 32;
  // Initialize array to zero. See more details at: https://stackoverflow.com/a/5591516.
  char buf[kBufSize] = {};

  uint64_t pc = 0;
  auto* codes = const_cast<uint8_t*>(byte_code.data());
  size_t codes_size = byte_code.size();
  int inst_size = 0;

  std::vector<uint64_t> res;
  do {
    if (IsRetInst(*codes)) {
      res.push_back(pc);
    }
    // TODO(yzhao): MCDisassembler::getInst() works better here, because it returns a MCInst, with
    // an opcode for examination. Unfortunately, MCDisassembler is difficult to create without
    // class LLVMDisasmContex, which is not exposed.
    inst_size =
        LLVMDisasmInstruction(kLLVMDisasmContext.ref(), codes, codes_size, pc, buf, kBufSize);

    pc += inst_size;
    codes += inst_size;
    codes_size -= inst_size;
  } while (inst_size != 0);
  return res;
}

}  // namespace

StatusOr<std::vector<uint64_t>> ElfReader::FuncRetInstAddrs(const SymbolInfo& func_symbol) {
  PL_ASSIGN_OR_RETURN(utils::u8string byte_code, FuncByteCode(func_symbol));
  std::vector<uint64_t> addrs = FindRetInsts(byte_code);
  for (auto& offset : addrs) {
    offset += func_symbol.address;
  }
  return addrs;
}

StatusOr<utils::u8string> ElfReader::FuncByteCode(const SymbolInfo& func_symbol) {
  constexpr char kDotText[] = ".text";
  ELFIO::section* text_section = nullptr;
  for (int i = 0; i < elf_reader_.sections.size(); ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_name() == kDotText) {
      text_section = psec;
      break;
    }
  }
  if (text_section == nullptr) {
    return error::NotFound("Could not find section=$0 in binary=$1", kDotText, binary_path_);
  }
  int offset = func_symbol.address - text_section->get_address() + text_section->get_offset();

  std::ifstream ifs(binary_path_, std::ios::binary);
  if (!ifs.seekg(offset)) {
    return error::Internal("Failed to seek position=$0 in binary=$1", offset, binary_path_);
  }
  utils::u8string byte_code(func_symbol.size, '\0');
  auto* buf = reinterpret_cast<char*>(byte_code.data());
  if (!ifs.read(buf, func_symbol.size)) {
    return error::Internal("Failed to read size=$0 bytes from offset=$1 in binary=$2",
                           func_symbol.size, offset, binary_path_);
  }
  if (ifs.gcount() != static_cast<int64_t>(func_symbol.size)) {
    return error::Internal("Only read size=$0 bytes from offset=$1 in binary=$2, expect $3 bytes",
                           func_symbol.size, offset, binary_path_, ifs.gcount());
  }
  return byte_code;
}

StatusOr<absl::flat_hash_map<std::string, std::vector<std::string>>> ExtractGolangInterfaces(
    ElfReader* elf_reader) {
  absl::flat_hash_map<std::string, std::vector<std::string>> interface_types;

  // All itable objects in the symbols are prefixed with this string.
  const std::string_view kITablePrefix("go.itab.");

  PL_ASSIGN_OR_RETURN(
      std::vector<ElfReader::SymbolInfo> itable_symbols,
      elf_reader->SearchSymbols(kITablePrefix, SymbolMatchType::kPrefix, STT_OBJECT));

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
    interface_types[std::string(interface_name)].emplace_back(type);
  }

  return interface_types;
}

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
