#pragma once

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/TargetSelect.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace dwarf_tools {

class DwarfReader {
 public:
  /**
   * Creates a DwarfReader that provides access to DWARF Debugging information entries (DIEs).
   * @param obj_filename The object file from which to read DWARF information.
   * @return error if file does not exist or is not a valid object file. Otherwise returns
   * a unique pointer to a DwarfReader.
   */
  static StatusOr<std::unique_ptr<DwarfReader>> Create(std::string_view obj_filename);

  /**
   * Searches the debug information for Debugging information entries (DIEs)
   * that match the name.
   * @param name Search string, which must be an exact match.
   * @param type option DIE tag type on which to filter (e.g. look for structs).
   * @return Error if DIEs could not be searched, otherwise a vector of DIEs that match the search
   * string.
   */
  StatusOr<std::vector<llvm::DWARFDie>> GetMatchingDIEs(
      std::string_view name,
      llvm::dwarf::Tag type = static_cast<llvm::dwarf::Tag>(llvm::dwarf::DW_TAG_invalid));

 private:
  DwarfReader(std::unique_ptr<llvm::MemoryBuffer> buffer,
              std::unique_ptr<llvm::DWARFContext> dwarf_context)
      : memory_buffer_(std::move(buffer)), dwarf_context_(std::move(dwarf_context)) {
    // TODO(oazizi): This might cause it to get called too many times. Check perf cost.
    // There is also a similar call in elf_tools.cc.
    llvm::InitializeNativeTarget();
  }

  static Status GetMatchingDIEs(llvm::DWARFContext::unit_iterator_range CUs, std::string_view name,
                                llvm::dwarf::Tag tag, std::vector<llvm::DWARFDie>* dies_out);

  std::unique_ptr<llvm::MemoryBuffer> memory_buffer_;
  std::unique_ptr<llvm::DWARFContext> dwarf_context_;
};

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
