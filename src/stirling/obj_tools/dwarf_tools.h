#pragma once

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/TargetSelect.h>

#include <absl/container/flat_hash_map.h>

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace dwarf_tools {

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

enum class VarType {
  kUnspecified = 0,
  kVoid,
  kBaseType,
  kPointer,
  kStruct,
  kSubroutine,
};

// Identifies where an argument is located when tracing a function entry:
// in memory (kStack), or in registers (kRegister).
enum class LocationType {
  kUnknown,
  kStack,
  kRegister,
};

struct StructMemberInfo {
  // Offset within the struct.
  uint64_t offset = std::numeric_limits<uint64_t>::max();
  VarType type = VarType::kUnspecified;
  std::string type_name = "";

  std::string ToString() const {
    return absl::Substitute("offset=$0 type=$1 type_name=$2", offset, magic_enum::enum_name(type),
                            type_name);
  }
};

struct ArgLocation {
  LocationType type = LocationType::kUnknown;
  int64_t offset = std::numeric_limits<uint64_t>::max();

  std::string ToString() const {
    return absl::Substitute("type=$0 offset=$1", magic_enum::enum_name(type), offset);
  }
};

struct ArgInfo {
  VarType type = VarType::kUnspecified;
  std::string type_name = "";
  ArgLocation location;

  // If true, this argument is really a return value.
  bool retarg = false;

  std::string ToString() const {
    return absl::Substitute("type=$0 type_name=$1 location=[$2] retarg=$3",
                            magic_enum::enum_name(type), type_name, location.ToString(), retarg);
  }
};

struct RetValInfo {
  VarType type = VarType::kUnspecified;
  std::string type_name = "";
  size_t byte_size = 0;

  std::string ToString() const {
    return absl::Substitute("type=$0 type_name=$1 byte_size=$2", magic_enum::enum_name(type),
                            type_name, byte_size);
  }
};

inline bool operator==(const StructMemberInfo& a, const StructMemberInfo& b) {
  return a.offset == b.offset && a.type == b.type && a.type_name == b.type_name;
}

inline bool operator==(const ArgLocation& a, const ArgLocation& b) {
  return a.type == b.type && a.offset == b.offset;
}

inline bool operator==(const ArgInfo& a, const ArgInfo& b) {
  return a.location == b.location && a.type == b.type && a.type_name == b.type_name &&
         a.retarg == b.retarg;
}

inline bool operator==(const RetValInfo& a, const RetValInfo& b) {
  return a.type == b.type && a.type_name == b.type_name && a.byte_size == b.byte_size;
}

inline std::ostream& operator<<(std::ostream& os, const StructMemberInfo& var_info) {
  os << var_info.ToString();
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const ArgLocation& x) {
  os << x.ToString();
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const ArgInfo& arg_info) {
  os << arg_info.ToString();
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const RetValInfo& ret_val_info) {
  os << ret_val_info.ToString();
  return os;
}

//-----------------------------------------------------------------------------
// DwarfReader
//-----------------------------------------------------------------------------

class DwarfReader {
 public:
  /**
   * Creates a DwarfReader that provides access to DWARF Debugging information entries (DIEs).
   * @param obj_filename The object file from which to read DWARF information.
   * @param index If true, creates an index to speed up accesses when called more than once.
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
   * Returns information about a member within a struct.
   * @param struct_name Full name of the struct.
   * @param member_name Name of member within the struct.
   * @return Error if member not found; otherwise a VarInfo struct.
   */
  StatusOr<StructMemberInfo> GetStructMemberInfo(std::string_view struct_name,
                                                 std::string_view member_name);

  /**
   * Returns the offset of a member within a struct.
   * @param struct_name Full name of the struct.
   * @param member_name Name of member within the struct.
   * @return Error if offset could not be found; otherwise, offset in bytes.
   */
  StatusOr<uint64_t> GetStructMemberOffset(std::string_view struct_name,
                                           std::string_view member_name) {
    PL_ASSIGN_OR_RETURN(StructMemberInfo member_info,
                        GetStructMemberInfo(struct_name, member_name));
    return member_info.offset;
  }

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
   * and may fail for certain functions. Compare this function to GetFunctionArgInfo().
   */
  StatusOr<ArgLocation> GetArgumentLocation(std::string_view function_symbol_name,
                                            std::string_view arg_name);

  /**
   * Returns information on the arguments of a function, including location and type.
   *
   * NOTE: Currently, the method used by this function to determine the argument offset
   * differs from the method used by GetArgumentLocation(), which uses the DW_AT_location
   * attribute. This function infers the location based on type sizes, and an implicit understanding
   * of the calling convention.
   * It is currently more robust for our uses cases, but eventually we should use the DW_AT_location
   * approach, which should be more generally robust (once we implement processing it correctly).
   */
  StatusOr<std::map<std::string, ArgInfo>> GetFunctionArgInfo(
      std::string_view function_symbol_name);

  /**
   * Returns information on the return value of a function. This works for C/C++.
   * Note that Golang return variables are treated as arguments to the function.
   */
  StatusOr<RetValInfo> GetFunctionRetValInfo(std::string_view function_symbol_name);

  bool IsValid() { return dwarf_context_->getNumCompileUnits() != 0; }

  const llvm::dwarf::SourceLanguage& source_language() const { return source_language_; }

 private:
  DwarfReader(std::unique_ptr<llvm::MemoryBuffer> buffer,
              std::unique_ptr<llvm::DWARFContext> dwarf_context);

  // Detects the source language of the dwarf content being read.
  Status DetectSourceLanguage();

  // Builds an index for certain commonly used DIE types (e.g. structs and functions).
  // When making multiple DwarfReader calls, this speeds up the process at the cost of some memory.
  void IndexDIEs();

  static Status GetMatchingDIEs(llvm::DWARFContext::unit_iterator_range CUs, std::string_view name,
                                std::optional<llvm::dwarf::Tag> tag,
                                std::vector<llvm::DWARFDie>* dies_out);

  // Records the source language of the DWARF information.
  llvm::dwarf::SourceLanguage source_language_;

  std::unique_ptr<llvm::MemoryBuffer> memory_buffer_;
  std::unique_ptr<llvm::DWARFContext> dwarf_context_;

  // Nested map: [tag][symbol_name] -> DWARFDie
  absl::flat_hash_map<llvm::dwarf::Tag, absl::flat_hash_map<std::string, llvm::DWARFDie>> die_map_;
};

//-----------------------------------------------------------------------------
// DWARFDie Functions (made public for test use only)
//-----------------------------------------------------------------------------

// Returns the DW_AT_name attribute of the input DIE. Returns an empty string if attribute does not
// exist, or for any errors.
std::string_view GetShortName(const llvm::DWARFDie& die);

// Returns the DW_AT_linkage_name attribute of the input DIE. Returns an empty string if attribute
// does not exist, or for any errors.
std::string_view GetLinkageName(const llvm::DWARFDie& die);

// Returns the text representation of the input DIE.
std::string Dump(const llvm::DWARFDie& die);

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
