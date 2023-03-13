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

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Support/TargetSelect.h>

#include <absl/container/flat_hash_map.h>

#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/abi_model.h"
#include "src/stirling/obj_tools/utils.h"

namespace px {
namespace stirling {
namespace obj_tools {

enum class VarType {
  // Refers to types that are not yet handled.
  kUnspecified = 0,

  kVoid,
  kBaseType,
  kPointer,
  kClass,
  kStruct,
  kSubroutine,
};

struct TypeInfo {
  VarType type = VarType::kUnspecified;

  // Describes the concrete type.
  // TODO(yzhao): Rename to impl_type (or just keep it).
  std::string type_name = "";

  // Describes the declared type, which is the symbolic type name declared in the source code of the
  // program. One such example is typedefs in C/C++.
  std::string decl_type = "";

  std::string ToString() const {
    return absl::Substitute("type=$0 decl_type=$1 type_name=$2", magic_enum::enum_name(type),
                            decl_type, type_name);
  }
};

struct StructMemberInfo {
  // Offset within the struct.
  uint64_t offset = std::numeric_limits<uint64_t>::max();
  TypeInfo type_info;

  std::string ToString() const {
    return absl::Substitute("offset=$0 type_info=[$1]", offset, type_info.ToString());
  }
};

struct ArgInfo {
  TypeInfo type_info;
  VarLocation location;

  // If true, this argument is really a return value.
  // Used by golang return values which are really function arguments from a DWARF perspective.
  bool retarg = false;

  std::string ToString() const {
    return absl::Substitute("type_info=[$0] location=[$1] retarg=$2", type_info.ToString(),
                            location.ToString(), retarg);
  }
};

struct RetValInfo {
  TypeInfo type_info;
  size_t byte_size = 0;

  std::string ToString() const {
    return absl::Substitute("type_info=[$0] byte_size=$1", type_info.ToString(), byte_size);
  }
};

struct StructSpecEntry {
  uint64_t offset = 0;
  uint64_t size = 0;
  TypeInfo type_info;

  // The path encoding the original data source structure, before flattening.
  // This field uses "JSON pointer" syntax to simplify decoding the data into JSON.
  // https://rapidjson.org/md_doc_pointer.html
  std::string path;

  std::string ToString() const {
    return absl::Substitute("offset=$0 size=$1 type_info=[$2] path=$3", offset, size,
                            type_info.ToString(), path);
  }
};

inline bool operator==(const TypeInfo& a, const TypeInfo& b) {
  return a.type == b.type && a.type_name == b.type_name;
}

inline bool operator==(const StructMemberInfo& a, const StructMemberInfo& b) {
  return a.offset == b.offset && a.type_info == b.type_info;
}

inline bool operator==(const VarLocation& a, const VarLocation& b) {
  return a.loc_type == b.loc_type && a.offset == b.offset && a.registers == b.registers;
}

inline bool operator==(const ArgInfo& a, const ArgInfo& b) {
  return a.location == b.location && a.type_info == b.type_info && a.retarg == b.retarg;
}

inline bool operator==(const RetValInfo& a, const RetValInfo& b) {
  return a.type_info == b.type_info && a.byte_size == b.byte_size;
}

inline bool operator==(const StructSpecEntry& a, const StructSpecEntry& b) {
  return a.offset == b.offset && a.size == b.size && a.type_info == b.type_info && a.path == b.path;
}

/**
 * Accepts an executable and reads DWARF information from it.
 * APIs are provided for accessing the needed data.
 */
class DwarfReader {
 public:
  /**
   * Creates a DwarfReader that provides access to DWARF Debugging information entries (DIEs).
   * @param obj_filename The object file from which to read DWARF information.
   * @param index If true, creates an index to speed up accesses when called more than once.
   * @return error if file does not exist or is not a valid object file. Otherwise returns
   * a unique pointer to a DwarfReader.
   */
  static StatusOr<std::unique_ptr<DwarfReader>> CreateWithoutIndexing(
      const std::filesystem::path& path);
  static StatusOr<std::unique_ptr<DwarfReader>> CreateIndexingAll(
      const std::filesystem::path& path);
  static StatusOr<std::unique_ptr<DwarfReader>> CreateWithSelectiveIndexing(
      const std::filesystem::path& path, const std::vector<SymbolSearchPattern>& symbol_patterns);

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
   * Return the size of a struct.
   */
  StatusOr<uint64_t> GetStructByteSize(std::string_view struct_name);

  /**
   * Returns information about a member within a struct.
   * @param struct_name Full name of the struct.
   * @param member_name Name of member within the struct.
   * @return Error if member not found; otherwise a VarInfo struct.
   */
  StatusOr<StructMemberInfo> GetStructMemberInfo(std::string_view struct_name, llvm::dwarf::Tag tag,
                                                 std::string_view member_name,
                                                 llvm::dwarf::Tag member_tag);

  /**
   * Returns the offset of a member within a struct.
   * @param struct_name Full name of the struct.
   * @param member_name Name of member within the struct.
   * @return Error if offset could not be found; otherwise, offset in bytes.
   */
  StatusOr<uint64_t> GetStructMemberOffset(std::string_view struct_name,
                                           std::string_view member_name) {
    PX_ASSIGN_OR_RETURN(StructMemberInfo member_info,
                        GetStructMemberInfo(struct_name, llvm::dwarf::DW_TAG_structure_type,
                                            member_name, llvm::dwarf::DW_TAG_member));
    return member_info.offset;
  }

  StatusOr<uint64_t> GetClassMemberOffset(std::string_view class_name,
                                          std::string_view member_name) {
    PX_ASSIGN_OR_RETURN(StructMemberInfo member_info,
                        GetStructMemberInfo(class_name, llvm::dwarf::DW_TAG_class_type, member_name,
                                            llvm::dwarf::DW_TAG_member));
    return member_info.offset;
  }

  StatusOr<uint64_t> GetClassParentOffset(std::string_view class_name,
                                          std::string_view parent_name) {
    PX_ASSIGN_OR_RETURN(StructMemberInfo member_info,
                        GetStructMemberInfo(class_name, llvm::dwarf::DW_TAG_class_type, parent_name,
                                            llvm::dwarf::DW_TAG_inheritance));
    return member_info.offset;
  }

  /**
   * Returns a struct spec, defining all the base type members of a struct in a flattened form.
   * For use to decode raw bytes (which are also flat).
   *
   * Each base type has:
   *  - offset: where the data is located in the raw bytes.
   *  - type_info: the type of the field (useful for knowing how to interpret the bytes).
   *  - path: encodes the source of the data using JSON pointer syntax,
   *                  so a raw struct data block can be easily decoded into JSON.
   *
   * offset=0 size=8 type_info=[type=kBaseType type_name=long int] path=.O0
   * offset=8 size=1 type_info=[type=kBaseType type_name=bool] path=.O1.M0.L0
   * offset=12 size=4 type_info=[type=kBaseType type_name=int] path=.O1.M0.L1
   * offset=16 size=8 type_info=[type=kPointer type_name=long int*] path=.O1.M0.L2
   * offset=24 size=1 type_info=[type=kBaseType type_name=bool] path=.O1.M1
   * offset=32 size=1 type_info=[type=kBaseType type_name=bool] path=.O1.M2.L0
   * offset=36 size=4 type_info=[type=kBaseType type_name=int] path=.O1.M2.L1
   *
   * @param struct_name Full name of the struct.
   * @return vector of entries in order of layout (offset field is monotonically increasing).
   */
  StatusOr<std::vector<StructSpecEntry>> GetStructSpec(std::string_view struct_name);

  /**
   * Returns the type pointed to by a pointer type.
   */
  StatusOr<TypeInfo> DereferencePointerType(std::string type_name);

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
  StatusOr<VarLocation> GetArgumentLocation(std::string_view function_symbol_name,
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

  bool IsValid() const { return dwarf_context_->getNumCompileUnits() != 0; }

  const llvm::dwarf::SourceLanguage& source_language() const { return source_language_; }
  const std::string& compiler() const { return compiler_; }

 private:
  DwarfReader(std::unique_ptr<llvm::MemoryBuffer> buffer,
              std::unique_ptr<llvm::DWARFContext> dwarf_context);

  // Detects the source language of the dwarf content being read.
  Status DetectSourceLanguage();

  // Builds an index for certain commonly used DIE types (e.g. structs and functions).
  // When making multiple DwarfReader calls, this speeds up the process at the cost of some memory.
  //
  // If the search patterns are not provided, all DIEs of the matching tags are indexed.
  // Otherwise, only the ones whose names match are indexed.
  void IndexDIEs(const std::optional<std::vector<SymbolSearchPattern>>& symbol_search_patterns_opt);

  // Walks the struct_die for all members, recursively visiting any members which are also structs,
  // to capture information of all base type members of the struct in a flattened form.
  // See GetStructSpec() for the public interface, and the output format.
  Status FlattenedStructSpec(const llvm::DWARFDie& struct_die, std::vector<StructSpecEntry>* output,
                             const std::string& path_prefix, int offset);

  void InsertToDIEMap(std::string name, llvm::dwarf::Tag tag, llvm::DWARFDie die);
  std::optional<llvm::DWARFDie> FindInDIEMap(const std::string& name, llvm::dwarf::Tag tag) const;

  // Records the source language of the DWARF information.
  llvm::dwarf::SourceLanguage source_language_;

  // Records the name of the compiler that produces this file.
  std::string compiler_;

  std::unique_ptr<llvm::MemoryBuffer> memory_buffer_;
  std::unique_ptr<llvm::DWARFContext> dwarf_context_;

  // Nested map: [tag][symbol_name] -> DWARFDie
  absl::flat_hash_map<llvm::dwarf::Tag, absl::flat_hash_map<std::string, llvm::DWARFDie>> die_map_;
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
