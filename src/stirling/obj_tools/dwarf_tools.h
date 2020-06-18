#pragma once

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/TargetSelect.h>

#include <absl/container/flat_hash_map.h>

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
   * @param index If true, creates an index of struct tags, to speed up GetStructMemberOffset() when
   * called more than once.
   * @return error if file does not exist or is not a valid object file. Otherwise returns
   * a unique pointer to a DwarfReader.
   */
  static StatusOr<std::unique_ptr<DwarfReader>> Create(std::string_view obj_filename,
                                                       bool index = true);

  /**
   * Searches the debug information for Debugging information entries (DIEs)
   * that match the name.
   * @param name Search string, which must be an exact match.
   * @param type option DIE tag type on which to filter (e.g. look for structs).
   * @return Error if DIEs could not be searched, otherwise a vector of DIEs that match the search
   * string.
   */
  StatusOr<std::vector<llvm::DWARFDie>> GetMatchingDIEs(std::string_view name,
                                                        std::optional<llvm::dwarf::Tag> type = {});

  /**
   * Like GetMatchingDIEs, but returns error if there is not exactly one match.
   */
  StatusOr<llvm::DWARFDie> GetMatchingDIE(std::string_view name,
                                          std::optional<llvm::dwarf::Tag> type = {});

  /**
   * Returns the offset of a member within a struct.
   * @param struct_name Full name of the struct.
   * @param member_name Name of member within the struct.
   * @return Error if offset could not be found; otherwise, offset in bytes.
   */
  StatusOr<uint64_t> GetStructMemberOffset(std::string_view struct_name,
                                           std::string_view member_name);

  StatusOr<uint64_t> GetArgumentTypeByteSize(std::string_view symbol_name,
                                             std::string_view arg_name);

  StatusOr<int64_t> GetArgumentStackPointerOffset(std::string_view symbol_name,
                                                  std::string_view arg_name);

  bool IsValid() { return dwarf_context_->getNumCompileUnits() != 0; }

 private:
  DwarfReader(std::unique_ptr<llvm::MemoryBuffer> buffer,
              std::unique_ptr<llvm::DWARFContext> dwarf_context);

  // Builds an index for struct types. If making multiple calls to GetStructMemberOffset,
  // this should speed up the process at the cost of some more memory.
  void IndexStructs();

  static Status GetMatchingDIEs(llvm::DWARFContext::unit_iterator_range CUs, std::string_view name,
                                std::optional<llvm::dwarf::Tag> tag,
                                std::vector<llvm::DWARFDie>* dies_out);

  std::unique_ptr<llvm::MemoryBuffer> memory_buffer_;
  std::unique_ptr<llvm::DWARFContext> dwarf_context_;

  absl::flat_hash_map<std::string, llvm::DWARFDie> die_struct_map_;
};

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
