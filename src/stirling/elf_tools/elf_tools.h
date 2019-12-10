#pragma once

#include <memory>
#include <string>
#include <vector>

#include <elfio/elfio.hpp>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace elf_tools {

enum class SymbolMatchType {
  // Search for the symbol that is an exact match with the search string.
  kExact,

  // Search for a symbol match that ends with the search string.
  kSuffix,

  // Search for a symbol that contains the search string.
  kAny
};

class ElfReader {
 public:
  /**
   * Load a new binary for analysis.
   * @param binary_path Path to the binary to read.
   * @return error if could not setup elf reader.
   */
  static StatusOr<std::unique_ptr<ElfReader>> Create(const std::string& binary_path);

  /**
   * Returns a list of symbol names that meets the search criteria.
   *
   * @param search_symbol The symbol to search for.
   * @param match_type Type of search (e.g. exact match, subtring, suffix).
   */
  StatusOr<std::vector<std::string>> ListSymbols(std::string_view search_symbol,
                                                 SymbolMatchType match_type);

  /**
   * Returns the address of the symbol.
   *
   * @param binary_path binary in which to search for the symbol.
   * @param symbol the symbol to search for.
   * @return the address of the symbol.
   * An error is returned if either the binary path is invalid or the symbol could not be found.
   */
  StatusOr<int64_t> SymbolAddress(std::string_view symbol);

 private:
  ElfReader() {}

  // Set up an elf reader, so we can extract debug symbols.
  ELFIO::elfio elf_reader_;
};

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
