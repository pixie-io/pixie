#include "src/stirling/obj_tools/elf_tools.h"

#include <set>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/utils.h"
#include "src/common/fs/fs_wrapper.h"

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
  auto elf_reader = std::unique_ptr<ElfReader>(new ElfReader);

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
    VLOG(1) << absl::Substitute("$0 $1", psec->get_type(), psec->get_name());

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

std::vector<std::string> ElfReader::ListSymbols(std::string_view search_symbol,
                                                SymbolMatchType match_type) {
  std::vector<std::string> symbol_names;
  std::set<ELFIO::Elf64_Addr> symbol_addrs;

  // Scan all sections to find the symbol table (SHT_SYMTAB)
  ELFIO::Elf_Half sec_num = elf_reader_.sections.size();
  for (int i = 0; i < sec_num; ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_type() != SHT_SYMTAB) {
      continue;
    }

    // Scan all symbols inside the symbol table.
    const ELFIO::symbol_section_accessor symbols(elf_reader_, psec);
    for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
      std::string name;
      ELFIO::Elf64_Addr addr;
      ELFIO::Elf_Xword size;
      unsigned char bind;
      unsigned char type = STT_NOTYPE;
      ELFIO::Elf_Half section_index;
      unsigned char other;
      symbols.get_symbol(j, name, addr, size, bind, type, section_index, other);

      // Only consider function symbols.
      if (type != STT_FUNC) {
        continue;
      }

      // Check for symbol match.
      bool match = false;
      switch (match_type) {
        case SymbolMatchType::kExact:
          match = (name == search_symbol);
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

      // Symbol address has already been seen.
      // Note that multiple symbols can point to the same address.
      // But symbol names cannot be duplicate.
      if (symbol_addrs.find(addr) != symbol_addrs.end()) {
        continue;
      }
      symbol_addrs.insert(addr);

      symbol_names.push_back(std::move(name));
    }
  }
  return symbol_names;
}

std::optional<int64_t> ElfReader::SymbolAddress(std::string_view symbol) {
  // Scan all sections to find the symbol table (SHT_SYMTAB)
  ELFIO::Elf_Half sec_num = elf_reader_.sections.size();
  for (int i = 0; i < sec_num; ++i) {
    ELFIO::section* psec = elf_reader_.sections[i];
    if (psec->get_type() != SHT_SYMTAB) {
      continue;
    }

    // Scan all symbols inside the symbol table.
    const ELFIO::symbol_section_accessor symbols(elf_reader_, psec);
    for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
      std::string name;
      ELFIO::Elf64_Addr addr = 0;
      ELFIO::Elf_Xword size;
      unsigned char bind;
      unsigned char type;
      ELFIO::Elf_Half section_index;
      unsigned char other;
      symbols.get_symbol(j, name, addr, size, bind, type, section_index, other);

      if (symbol == name) {
        return addr;
      }
    }
  }
  return {};
}

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
