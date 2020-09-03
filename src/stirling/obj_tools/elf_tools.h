#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "third_party/ELFIO/elfio/elfio.hpp"

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace elf_tools {

enum class SymbolMatchType {
  // Search for a symbol that is an exact match of the search string.
  kExact,

  // Search for a symbol that starts with the search string.
  kPrefix,

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
      const std::string& binary_path,
      const std::filesystem::path& debug_file_dir = "/usr/lib/debug");

  std::filesystem::path& debug_symbols_path() { return debug_symbols_path_; }

  struct SymbolInfo {
    std::string name;
    int type = -1;
    uint64_t address = -1;
    uint64_t size = -1;
  };

  /**
   * Returns a list of symbol names that meets the search criteria.
   *
   * @param search_symbol The symbol to search for.
   * @param match_type Type of search (e.g. exact match, substring, suffix).
   * @param Symbol type (e.g. STT_FUNC, STT_OBJECT, ...). See uapi/linux/elf.h.
   */
  StatusOr<std::vector<SymbolInfo>> SearchSymbols(std::string_view search_symbol,
                                                  SymbolMatchType match_type,
                                                  std::optional<int> symbol_type = std::nullopt);

  /**
   * Like SearchSymbols, but for function symbols only.
   */
  StatusOr<std::vector<SymbolInfo>> ListFuncSymbols(std::string_view search_symbol,
                                                    SymbolMatchType match_type);

  /**
   * Returns the address of the specified symbol, if found.
   *
   * @param binary_path binary in which to search for the symbol.
   * @param symbol The symbol to search for, as an exact match.
   * @return The address of the symbol or empty if symbol could not be found.
   */
  std::optional<int64_t> SymbolAddress(std::string_view symbol);

  /**
   * Returns the address of the return instructions of the function.
   */
  StatusOr<std::vector<uint64_t>> FuncRetInstAddrs(const SymbolInfo& func_symbol);

 private:
  ElfReader() = default;

  /**
   * Locates the debug symbols for the currently loaded ELF object.
   * External symbols are discovered using either the build-id or the debug-link.
   *
   * @param debug_file_dir The system location where debug symbols are located.
   * @return The path to the debug symbols, which may be the binary itself, or an external file.
   */
  Status LocateDebugSymbols(const std::filesystem::path& debug_file_dir = "/usr/lib/debug");

  /**
   * Returns the byte code of the function specified by the symbol.
   */
  StatusOr<pl::utils::u8string> FuncByteCode(const SymbolInfo& func_symbol);

  std::string binary_path_;

  std::filesystem::path debug_symbols_path_;

  // Set up an elf reader, so we can extract debug symbols.
  ELFIO::elfio elf_reader_;
};

/**
 * Returns a map of all interfaces, and types that implement that interface in a go binary
 */
StatusOr<absl::flat_hash_map<std::string, std::vector<std::string>>> ExtractGolangInterfaces(
    elf_tools::ElfReader* elf_reader);

}  // namespace elf_tools
}  // namespace stirling
}  // namespace pl
