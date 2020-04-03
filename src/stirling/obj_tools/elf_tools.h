#pragma once

#include <memory>
#include <string>
#include <vector>

#include "third_party/ELFIO/elfio/elfio.hpp"

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace elf_tools {

enum class SymbolMatchType {
  // Search for a symbol that is an exact match of the search string.
  kExact,

  // Search for a symbol that ends with the search string.
  kSuffix,

  // Search for a symbol that contains the search string.
  kSubstr
};

class ElfReader {
 public:
  /**
   * Load a new binary for analysis.
   *
   * @param binary_path Path to the binary to read.
   * @param debug_file_dir Location of external debug files.
   * @return error if could not setup elf reader.
   */
  static StatusOr<std::unique_ptr<ElfReader>> Create(
      const std::string& binary_path, std::string_view debug_file_dir = "/usr/lib/debug");

  /**
   * Returns a list of symbol names that meets the search criteria.
   *
   * @param search_symbol The symbol to search for.
   * @param match_type Type of search (e.g. exact match, subtring, suffix).
   */
  std::vector<std::string> ListSymbols(std::string_view search_symbol, SymbolMatchType match_type);

  /**
   * Returns the address of the specified symbol, if found.
   *
   * @param binary_path binary in which to search for the symbol.
   * @param symbol The symbol to search for, as an exact match.
   * @return The address of the symbol or empty if symbol could not be found.
   */
  std::optional<int64_t> SymbolAddress(std::string_view symbol);

 private:
  struct SymbolInfo {
    std::string name;
    int type = -1;
    uint64_t address = -1;
    uint64_t size = -1;
  };

  ElfReader() = default;

  /**
   * Returns a list of symbol names that meets the search criteria.
   * Use -1 for symbol_type to search all symbol types.
   */
  StatusOr<std::vector<SymbolInfo>> SearchSymbols(std::string_view pattern,
                                                  SymbolMatchType match_type, int symbol_type = -1);

  std::string binary_path_;

  // Set up an elf reader, so we can extract debug symbols.
  ELFIO::elfio elf_reader_;
};

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
