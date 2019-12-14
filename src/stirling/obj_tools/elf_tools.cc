#include "src/stirling/obj_tools/elf_tools.h"

#include <set>
#include <utility>

namespace pl {
namespace stirling {
namespace elf_tools {

// See http://elfio.sourceforge.net/elfio.pdf for examples of how to use ELFIO.

StatusOr<std::unique_ptr<ElfReader>> ElfReader::Create(const std::string& binary_path) {
  auto elf_reader = std::unique_ptr<ElfReader>(new ElfReader);

  if (!elf_reader->elf_reader_.load(binary_path)) {
    return error::Internal("Can't find or process ELF file $0", binary_path);
  }

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

      symbol_names.push_back(std::move(name));
      symbol_addrs.insert(addr);
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
