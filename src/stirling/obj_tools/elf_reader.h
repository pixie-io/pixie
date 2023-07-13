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

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>

#include <elfio/elfio.hpp>

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/utils.h"

using ::px::utils::u8string;

namespace px {
namespace stirling {
namespace obj_tools {

class ElfReader {
 public:
  /**
   * Load a new binary for analysis.
   * All addresses handled by the ElfReader are so called "binary" addresses (i.e. what `nm` would
   * return for a symbol), as opposed to virtual addresses. The difference only matters for PIE
   * binaries.
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
    // SymbolInfo always contains the so called "binary" address of the symbol (i.e. what `nm` would
    // return for the symbol).
    uint64_t address = -1;
    uint64_t size = -1;

    std::string ToString() const {
      return absl::Substitute("name=$0 type=$1 address=$2 size =$3", name, type,
                              absl::StrFormat("%x", address), size);
    }
  };

  StatusOr<int32_t> FindSegmentOffsetOfSection(std::string_view section_name);

  /**
   * Returns a list of symbol names that meets the search criteria.
   *
   * @param search_symbol The symbol to search for.
   * @param match_type Type of search (e.g. exact match, substring, suffix).
   * @param Symbol type (e.g. STT_FUNC, STT_OBJECT, ...). See uapi/linux/elf.h.
   * @param stop_at_first_match If true, stop the search at the first matched symbol.
   */
  StatusOr<std::vector<SymbolInfo>> SearchSymbols(std::string_view search_symbol,
                                                  SymbolMatchType match_type,
                                                  std::optional<int> symbol_type = std::nullopt,
                                                  bool stop_at_first_match = false);

  // Returns a unique symbol.
  StatusOr<SymbolInfo> SearchTheOnlySymbol(std::string_view symbol);

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
   * Looks up the symbol for an address.
   *
   * @param addr The symbol address to lookup.
   * @return Symbol name if address was found in the symbol table.
   *         std::nullopt if search completed by address was not found.
   *         Error if search failed to run as expected.
   *
   */
  StatusOr<std::optional<std::string>> AddrToSymbol(size_t addr);

  /**
   * Looks up the symbol for an instruction address.
   * Unlike AddrToSymbol, this function covers the entirety of the function body.
   * Any address in the body of the function is resolved, not just where the symbol is located.
   *
   * @param addr The symbol address to lookup.
   * @return Symbol name if address was found in the symbol table.
   *         std::nullopt if search completed by address was not found.
   *         Error if search failed to run as expected.
   */
  StatusOr<std::optional<std::string>> InstrAddrToSymbol(size_t addr);

  class Symbolizer {
   public:
    /**
     * Associate the address range [addr, addr+size] with the provided symbol name.
     * No checking is performed for overlapping regions, which will result in undefined behavior.
     */
    void AddEntry(uintptr_t addr, size_t size, std::string name);

    /**
     * Lookup the symbol for the specified address.
     */
    std::string_view Lookup(uintptr_t addr) const;

   private:
    struct SymbolAddrInfo {
      size_t size;
      std::string name;
    };

    // Key is an address.
    absl::btree_map<uintptr_t, SymbolAddrInfo> symbols_;
  };

  StatusOr<std::unique_ptr<Symbolizer>> GetSymbolizer();

  /**
   * Returns the address of the return instructions of the function.
   */
  StatusOr<std::vector<uint64_t>> FuncRetInstAddrs(const SymbolInfo& func_symbol);

  /**
   * Returns the byte code for the symbol at the specified section.
   */
  StatusOr<u8string> SymbolByteCode(std::string_view section, const SymbolInfo& symbol);

  /**
   * Returns the virtual address in the ELF file of offset 0x0. Calculated by finding the first
   * loadable segment and returning its virtual address minus its file offset.
   */
  StatusOr<uint64_t> GetVirtualAddrAtOffsetZero();

  /**
   * Returns the ELF section with the corresponding name
   */
  StatusOr<ELFIO::section*> SectionWithName(std::string_view section_name);

  /**
   * Returns the ELF type of this binary. (eg. ELFIO::ET_EXEC or ELFIO::ET_DYN).
   */
  ELFIO::Elf_Half ELFType();

  /**
   * Returns the byte code of the data within the binary at the specified offset
   */
  template <typename TCharType = u8string::value_type>
  StatusOr<std::basic_string<TCharType>> BinaryByteCode(size_t offset, size_t length) {
    std::ifstream ifs(binary_path_, std::ios::binary);
    if (!ifs.seekg(offset)) {
      return error::Internal("Failed to seek position=$0 in binary=$1", offset, binary_path_);
    }
    std::basic_string<TCharType> byte_code(length, '\0');
    auto* buf = reinterpret_cast<char*>(byte_code.data());
    if (!ifs.read(buf, length)) {
      return error::Internal("Failed to read size=$0 bytes from offset=$1 in binary=$2", length,
                             offset, binary_path_);
    }
    return byte_code;
  }

 private:
  ElfReader() = default;

  StatusOr<ELFIO::section*> SymtabSection();

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
  StatusOr<px::utils::u8string> FuncByteCode(const SymbolInfo& func_symbol);

  std::string binary_path_;

  std::filesystem::path debug_symbols_path_;

  // Set up an elf reader, so we can extract debug symbols.
  ELFIO::elfio elf_reader_;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
