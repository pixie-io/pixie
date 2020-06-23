#pragma once

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/TargetSelect.h>

#include <absl/container/flat_hash_map.h>

#include <map>
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

  /**
   * Returns the size (in bytes) for the type of a function argument.
   */
  StatusOr<uint64_t> GetArgumentTypeByteSize(std::string_view function_symbol_name,
                                             std::string_view arg_name);

  /**
   * Returns the location of a function argument relative to the stack pointer.
   * Note that there are differences in what different languages consider to be the stack pointer.
   * Golang returns positive numbers (i.e. considers the offset relative to the frame base,
   * or, in other words, the stack pointer before the frame has been created).
   * C++ functions return negative numbers (i.e. offset relative to the stack pointer
   * after the frame has been created).
   * NOTE: This function currently uses the DW_AT_location. It is NOT yet robust,
   * and may fail for certain functions. Compare this function to GetFunctionArgOffsets().
   */
  StatusOr<int64_t> GetArgumentStackPointerOffset(std::string_view function_symbol_name,
                                                  std::string_view arg_name);

  /**
   * Returns the location of all the arguments of a function.
   * NOTE: Currently, the method used by this function differs from the method used by
   * GetArgumentStackPointerOffset(), which uses the DW_AT_location attribute.
   * This function infers the location based on type sizes, and an implicit understanding
   * of the calling convention.
   * It is currently more robust for our uses cases, but eventually we should use the DW_AT_location
   * approach, which should be more generally robust (once we implement processing it correctly).
   */
  StatusOr<std::map<std::string, uint64_t>> GetFunctionArgOffsets(
      std::string_view function_symbol_name);

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
