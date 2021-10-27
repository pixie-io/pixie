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

#include "src/stirling/obj_tools/dwarf_reader.h"

#include <algorithm>

#include <llvm/DebugInfo/DIContext.h>
#include <llvm/Object/ObjectFile.h>

#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/obj_tools/abi_model.h"
#include "src/stirling/obj_tools/dwarf_utils.h"
#include "src/stirling/obj_tools/init.h"

namespace px {
namespace stirling {
namespace obj_tools {

using llvm::DWARFContext;
using llvm::DWARFDie;
using llvm::DWARFFormValue;

namespace {

std::vector<DWARFDie> GetParamDIEs(const DWARFDie& function_die) {
  std::vector<DWARFDie> dies;
  for (auto& die : function_die.children()) {
    if (die.getTag() == llvm::dwarf::DW_TAG_formal_parameter) {
      dies.push_back(std::move(die));
    }
  }
  return dies;
}

}  // namespace

// This will break on 32-bit binaries.
// Use ELF to get the correct value.
// https://superuser.com/questions/791506/how-to-determine-if-a-linux-binary-file-is-32-bit-or-64-bit
uint8_t kAddressSize = sizeof(void*);

StatusOr<std::unique_ptr<DwarfReader>> DwarfReader::Create(
    const std::filesystem::path& obj_file_path, bool index) {
  using llvm::MemoryBuffer;

  std::error_code ec;

  std::string obj_filename = obj_file_path.string();

  llvm::ErrorOr<std::unique_ptr<MemoryBuffer>> buff_or_err =
      MemoryBuffer::getFileOrSTDIN(obj_filename);
  ec = buff_or_err.getError();
  if (ec) {
    return error::Internal("DwarfReader $0: $1", ec.message(), obj_filename);
  }

  std::unique_ptr<MemoryBuffer> buffer = std::move(buff_or_err.get());
  llvm::Expected<std::unique_ptr<llvm::object::Binary>> bin_or_err =
      llvm::object::createBinary(*buffer);
  ec = errorToErrorCode(bin_or_err.takeError());
  if (ec) {
    return error::Internal("DwarfReader $0: $1", ec.message(), obj_filename);
  }

  auto* obj_file = llvm::dyn_cast<llvm::object::ObjectFile>(bin_or_err->get());
  if (!obj_file) {
    return error::Internal("Could not create DWARFContext.");
  }

  auto dwarf_reader = std::unique_ptr<DwarfReader>(
      new DwarfReader(std::move(buffer), DWARFContext::create(*obj_file)));

  PL_RETURN_IF_ERROR(dwarf_reader->DetectSourceLanguage());

  if (index) {
    dwarf_reader->IndexDIEs();
  }

  return dwarf_reader;
}

DwarfReader::DwarfReader(std::unique_ptr<llvm::MemoryBuffer> buffer,
                         std::unique_ptr<llvm::DWARFContext> dwarf_context)
    : memory_buffer_(std::move(buffer)), dwarf_context_(std::move(dwarf_context)) {
  // Only very first call will actually perform initialization.
  InitLLVMOnce();
}

namespace {

bool IsMatchingTag(std::optional<llvm::dwarf::Tag> tag, const DWARFDie& die) {
  return (!tag.has_value() || (tag == die.getTag()));
}

bool IsMatchingDIE(std::string_view name, std::optional<llvm::dwarf::Tag> tag,
                   const DWARFDie& die) {
  if (!IsMatchingTag(tag, die)) {
    // Not the right type.
    return false;
  }

  const char* die_short_name = die.getName(llvm::DINameKind::ShortName);

  // May also want to consider the linkage name (e.g. the mangled name).
  // That is what llvm-dwarfdebug appears to do.
  // const char* die_linkage_name = die.getName(llvm::DINameKind::LinkageName);

  return (die_short_name && name == die_short_name);
}

}  // namespace

Status DwarfReader::GetMatchingDIEs(DWARFContext::unit_iterator_range CUs, std::string_view name,
                                    std::optional<llvm::dwarf::Tag> tag,
                                    std::vector<DWARFDie>* dies_out) {
  for (const auto& CU : CUs) {
    for (const auto& Entry : CU->dies()) {
      DWARFDie die = {CU.get(), &Entry};
      if (IsMatchingDIE(name, tag, die)) {
        dies_out->push_back(std::move(die));
      }
    }
  }

  return Status::OK();
}

namespace {

bool IsIndexedType(llvm::dwarf::Tag tag) {
  switch (tag) {
    // To index more DW_TAG types, simply add the type here.
    case llvm::dwarf::DW_TAG_class_type:
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_subprogram:
      return true;
    default:
      return false;
  }
}

bool IsNamespace(llvm::dwarf::Tag tag) { return tag == llvm::dwarf::DW_TAG_namespace; }

}  // namespace

Status DwarfReader::DetectSourceLanguage() {
  for (size_t i = 0; i < dwarf_context_->getNumCompileUnits(); ++i) {
    auto lang_opt =
        dwarf_context_->getUnitAtIndex(i)->getUnitDIE().find(llvm::dwarf::DW_AT_language);
    if (!lang_opt.hasValue()) {
      // Found that node executable built from the source can have the following DIE returned by
      // getUnitAtIndex()->getUnitDie().
      // 0x0000000b: DW_TAG_partial_unit
      //               DW_AT_stmt_list   (0x00000000)
      //               DW_AT_comp_dir    ("/home/yzhao/src/node/out")
      // This is not a DW_TAG_compile_unit.
      continue;
    }
    source_language_ =
        static_cast<llvm::dwarf::SourceLanguage>(llvm::dwarf::toUnsigned(lang_opt, /*default*/ 0));
    return Status::OK();
  }
  return error::Internal(
      "Could not determine the source language of the DWARF info. DW_AT_language not found on "
      "any compilation unit.");
}

void DwarfReader::IndexDIEs() {
  DWARFContext::unit_iterator_range CUs = dwarf_context_->normal_units();

  absl::flat_hash_map<const llvm::DWARFDebugInfoEntry*, std::string> dwarf_entry_names;

  // Map from DW_AT_specification to DIE. Only DW_TAG_subprogram can have this attribute.
  // Also only applies to CPP binaries.
  absl::flat_hash_map<uint64_t, DWARFDie> fn_spec_offsets;

  for (const auto& CU : CUs) {
    for (const auto& Entry : CU->dies()) {
      DWARFDie die = {CU.get(), &Entry};

      if (die.isSubprogramDIE()) {
        auto spec_or =
            AdaptLLVMOptional(llvm::dwarf::toReference(die.find(llvm::dwarf::DW_AT_specification)),
                              "Could not find attribute DW_AT_specification");
        if (spec_or.ok()) {
          fn_spec_offsets[spec_or.ValueOrDie()] = die;
        }
      }

      // TODO(oazizi/yzhao): Change to use the demangled name of DW_AT_linkage_name as the key to
      // index the function DIE. That removes the need of using manually-assembled names (through
      // parent DIE).

      auto name = std::string(GetShortName(die));

      if (name.empty()) {
        continue;
      }

      llvm::dwarf::Tag tag = die.getTag();

      if (IsIndexedType(tag) ||
          // Namespace entry is processed here so that the name components can be generated.
          IsNamespace(tag)) {
        llvm::DWARFDie parent_die = die.getParent();

        if (parent_die.isValid()) {
          const llvm::DWARFDebugInfoEntry* entry = parent_die.getDebugInfoEntry();

          if (entry != nullptr) {
            auto iter = dwarf_entry_names.find(entry);
            if (iter != dwarf_entry_names.end()) {
              std::string_view parent_name = iter->second;
              name = absl::StrCat(parent_name, "::", name);
            }
          }
          dwarf_entry_names[die.getDebugInfoEntry()] = name;
        }

        if (IsIndexedType(tag)) {
          auto& die_type_map = die_map_[tag];

          // TODO(oazizi): What's the right way to deal with duplicate names?
          // Only appears to happen with structs like the following:
          //  ThreadStart, _IO_FILE, _IO_marker, G, in6_addr
          // So probably okay for now. But need to be wary of this.
          if (die_type_map.find(name) != die_type_map.end()) {
            VLOG(1) << "Duplicate name: " << name;
          }
          die_type_map[name] = die;
        }
      }
    }
  }

  auto& fn_dies = die_map_[llvm::dwarf::DW_TAG_subprogram];

  for (auto iter = fn_dies.begin(); iter != fn_dies.end(); ++iter) {
    uint64_t offset = iter->second.getOffset();
    auto spec_iter = fn_spec_offsets.find(offset);
    if (spec_iter == fn_spec_offsets.end()) {
      continue;
    }
    // Replace the DIE with the DW_TAG_subprogram die that has DW_AT_specification attribute.
    iter->second = spec_iter->second;
  }
}

StatusOr<std::vector<DWARFDie>> DwarfReader::GetMatchingDIEs(std::string_view name,
                                                             std::optional<llvm::dwarf::Tag> type) {
  DCHECK(dwarf_context_ != nullptr);
  std::vector<DWARFDie> dies;

  // Special case for types that are indexed.
  if (type.has_value() && !die_map_.empty()) {
    llvm::dwarf::Tag tag = type.value();
    if (IsIndexedType(tag)) {
      auto& die_type_map = die_map_[tag];
      auto iter = die_type_map.find(name);
      if (iter != die_type_map.end()) {
        return std::vector<DWARFDie>{iter->second};
      }

      // Indexing was on, but nothing was found, so return empty vector.
      return std::vector<DWARFDie>{};
    }
  }

  // When there is no index, fall-back to manual search.
  PL_RETURN_IF_ERROR(GetMatchingDIEs(dwarf_context_->normal_units(), name, type, &dies));

  return dies;
}

StatusOr<DWARFDie> DwarfReader::GetMatchingDIE(std::string_view name,
                                               std::optional<llvm::dwarf::Tag> type) {
  PL_ASSIGN_OR_RETURN(std::vector<DWARFDie> dies, GetMatchingDIEs(name, type));
  if (dies.empty()) {
    return error::Internal("Could not locate symbol name=$0", name);
  }
  if (dies.size() > 1) {
    return error::Internal("Found $0 matches, expect only 1.", dies.size());
  }

  return dies.front();
}

namespace {

struct SimpleBlock {
  llvm::dwarf::LocationAtom code;
  int64_t operand;
};

StatusOr<SimpleBlock> DecodeSimpleBlock(const llvm::ArrayRef<uint8_t>& block) {
  // Don't ask me why address size is 0. Just copied it from LLVM examples.
  llvm::DataExtractor data(
      llvm::StringRef(reinterpret_cast<const char*>(block.data()), block.size()),
      /* isLittleEndian */ true, /* AddressSize */ 0);
  llvm::DWARFExpression expr(data, kAddressSize, llvm::dwarf::DwarfFormat::DWARF64);

  auto iter = expr.begin();
  auto& operation = *iter;
  ++iter;
  if (iter != expr.end()) {
    return error::Internal("Operation includes more than one component. Not yet supported.");
  }

  SimpleBlock decoded_block;

  decoded_block.code = static_cast<llvm::dwarf::LocationAtom>(operation.getCode());
  decoded_block.operand = operation.getRawOperand(0);

  VLOG(1) << absl::Substitute("Decoded block: code=$0 operand=$1",
                              magic_enum::enum_name(decoded_block.code), decoded_block.operand);

  return decoded_block;
}

StatusOr<uint64_t> GetMemberOffset(const DWARFDie& die) {
  DCHECK(
      // Members, eg: member functions and variables in C++.
      die.getTag() == llvm::dwarf::DW_TAG_member ||
      // Parent class inherited from.
      die.getTag() == llvm::dwarf::DW_TAG_inheritance);

  const char* die_short_name = die.getName(llvm::DINameKind::ShortName);

  PL_ASSIGN_OR_RETURN(
      const DWARFFormValue& attr,
      AdaptLLVMOptional(die.find(llvm::dwarf::DW_AT_data_member_location),
                        "Found member, but could not find data_member_location attribute."));

  // We've run into a case where the offset is encoded as a block instead of a constant.
  // See section 7.5.5 of the spec: http://dwarfstd.org/doc/DWARF5.pdf
  // Or use llvm-dwarfdump on src/stirling/obj_tools/testdata/go/sockshop_payments_service to see an
  // example. This handles that case as long as the block is simple (which is what we've seen in
  // practice).
  if (attr.getForm() == llvm::dwarf::DW_FORM_block1) {
    PL_ASSIGN_OR_RETURN(llvm::ArrayRef<uint8_t> offset_block,
                        AdaptLLVMOptional(attr.getAsBlock(), "Could not extract block."));

    PL_ASSIGN_OR_RETURN(SimpleBlock decoded_offset_block, DecodeSimpleBlock(offset_block));

    if (decoded_offset_block.code == llvm::dwarf::LocationAtom::DW_OP_plus_uconst) {
      return decoded_offset_block.operand;
    }

    return error::Internal("Unsupported operand: $0",
                           magic_enum::enum_name(decoded_offset_block.code));
  }

  PL_ASSIGN_OR_RETURN(uint64_t offset,
                      AdaptLLVMOptional(attr.getAsUnsignedConstant(),
                                        absl::Substitute("Could not extract offset for member $0.",
                                                         die_short_name)));
  return offset;
}

StatusOr<DWARFDie> GetTypeAttribute(const DWARFDie& die) {
  PL_ASSIGN_OR_RETURN(
      const DWARFFormValue& type_attr,
      AdaptLLVMOptional(die.find(llvm::dwarf::DW_AT_type), "Could not find DW_AT_type."));
  return die.getAttributeValueAsReferencedDie(type_attr);
}

std::string_view GetTypeName(const DWARFDie& die) {
  auto die_or = GetTypeAttribute(die);
  if (die_or.ok()) {
    return GetShortName(die_or.ValueOrDie());
  }
  return {};
}

// Recursively resolve the type of the input DIE until reaching a leaf type that has no further
// alias.
StatusOr<DWARFDie> GetTypeDie(const DWARFDie& die) {
  DWARFDie type_die = die;
  do {
    PL_ASSIGN_OR_RETURN(type_die, GetTypeAttribute(type_die));
  } while (type_die.getTag() == llvm::dwarf::DW_TAG_typedef);

  return type_die;
}

VarType GetType(const DWARFDie& die) {
  DCHECK(die.isValid());

  switch (die.getTag()) {
    case llvm::dwarf::DW_TAG_pointer_type:
      return VarType::kPointer;
    case llvm::dwarf::DW_TAG_subroutine_type:
      return VarType::kSubroutine;
    case llvm::dwarf::DW_TAG_base_type:
      return VarType::kBaseType;
    case llvm::dwarf::DW_TAG_class_type:
      return VarType::kClass;
    case llvm::dwarf::DW_TAG_structure_type:
      return VarType::kStruct;
    case llvm::dwarf::DW_TAG_const_type: {
      auto type_die_or = GetTypeDie(die);
      if (!type_die_or.ok()) {
        return VarType::kUnspecified;
      }
      return GetType(type_die_or.ValueOrDie());
    }
    default:
      return VarType::kUnspecified;
  }
}

// Returns the name of the DIE, it could be type name, member name, inheritance name etc.
// DIE name is loosely defined such that the name is used to lookup and is easier for caller to
// specify.
StatusOr<std::string> GetDieName(const DWARFDie& die) {
  DCHECK(die.isValid());

  switch (die.getTag()) {
    case llvm::dwarf::DW_TAG_pointer_type: {
      std::string type_name(GetShortName(die));

      // C++ dwarf info doesn't have a name attached to the pointer types,
      // so follow to the type die and create the appropriate name.
      if (type_name.empty()) {
        PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));
        type_name = absl::StrCat(GetShortName(type_die), "*");
      }
      return type_name;
    }
    case llvm::dwarf::DW_TAG_base_type:
    case llvm::dwarf::DW_TAG_class_type:
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_typedef:
    case llvm::dwarf::DW_TAG_subroutine_type:
    case llvm::dwarf::DW_TAG_member:
      return std::string(GetShortName(die));
    case llvm::dwarf::DW_TAG_inheritance:
    case llvm::dwarf::DW_TAG_const_type:
      return std::string(GetTypeName(die));
    default:
      return error::Internal(
          "Could not get the name of the input DIE, because of unexpected tag, DIE: $0", Dump(die));
  }
}

StatusOr<TypeInfo> GetTypeInfo(const DWARFDie& die, const DWARFDie& type_die) {
  TypeInfo type_info;

  PL_ASSIGN_OR_RETURN(DWARFDie decl_type_die, GetTypeAttribute(die));
  PL_ASSIGN_OR_RETURN(type_info.decl_type, GetDieName(decl_type_die));

  type_info.type = GetType(type_die);

  if (type_info.type == VarType::kUnspecified) {
    return error::InvalidArgument(
        "Failed to get type information for type DIE, DIE type is not supported, DIE: $0",
        Dump(type_die));
  }

  PL_ASSIGN_OR_RETURN(type_info.type_name, GetDieName(type_die));

  return type_info;
}

StatusOr<DWARFFormValue> GetDieAttribute(const DWARFDie& die, llvm::dwarf::Attribute attribute) {
  return AdaptLLVMOptional(die.find(attribute),
                           absl::Substitute("Could not find attribute $0 in DIE $1",
                                            magic_enum::enum_name(attribute), GetShortName(die)));
}

StatusOr<uint64_t> GetBaseOrStructTypeByteSize(const DWARFDie& die) {
  DCHECK((die.getTag() == llvm::dwarf::DW_TAG_base_type) ||
         (die.getTag() == llvm::dwarf::DW_TAG_structure_type));

  PL_ASSIGN_OR_RETURN(const DWARFFormValue& byte_size_attr,
                      GetDieAttribute(die, llvm::dwarf::DW_AT_byte_size));

  PL_ASSIGN_OR_RETURN(uint64_t byte_size,
                      AdaptLLVMOptional(byte_size_attr.getAsUnsignedConstant(),
                                        absl::Substitute("Could not extract byte_size [die=$0].",
                                                         GetShortName(die))));

  return byte_size;
}

StatusOr<uint64_t> GetTypeByteSize(const DWARFDie& die) {
  DCHECK(die.isValid());

  switch (die.getTag()) {
    case llvm::dwarf::DW_TAG_pointer_type:
    case llvm::dwarf::DW_TAG_subroutine_type:
      return kAddressSize;
    case llvm::dwarf::DW_TAG_base_type:
    case llvm::dwarf::DW_TAG_structure_type:
      return GetBaseOrStructTypeByteSize(die);
    default:
      return error::Internal(absl::Substitute("GetTypeByteSize - Unexpected DIE type: $0",
                                              magic_enum::enum_name(die.getTag())));
  }
}

StatusOr<uint64_t> GetAlignmentByteSize(const DWARFDie& die) {
  DCHECK(die.isValid());

  switch (die.getTag()) {
    case llvm::dwarf::DW_TAG_pointer_type:
    case llvm::dwarf::DW_TAG_subroutine_type:
      return kAddressSize;
    case llvm::dwarf::DW_TAG_base_type:
      return GetBaseOrStructTypeByteSize(die);
    case llvm::dwarf::DW_TAG_structure_type: {
      uint64_t max_size = 1;
      for (const auto& member_die : die.children()) {
        if ((member_die.getTag() == llvm::dwarf::DW_TAG_member)) {
          PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(member_die));
          PL_ASSIGN_OR_RETURN(uint64_t alignment_size, GetAlignmentByteSize(type_die));
          max_size = std::max(alignment_size, max_size);
        }
      }
      return max_size;
    }
    default:
      return error::Internal(
          absl::Substitute("Failed to get alignment size, unexpected DIE type: $0",
                           magic_enum::enum_name(die.getTag())));
  }
}

// This function calls out any die with DW_AT_variable_parameter == 0x1 to be a return value.
// The documentation on how Golang sets this field is sparse, so not sure if this is the
// right flag to look at.
// From examining a few cases, this interpretation of the flag seems to work, but it might
// also be completely wrong.
// TODO(oazizi): Find a better way to determine return values.
StatusOr<bool> IsGolangRetArg(const DWARFDie& die) {
  PL_ASSIGN_OR_RETURN(
      const DWARFFormValue& attr,
      AdaptLLVMOptional(die.find(llvm::dwarf::DW_AT_variable_parameter),
                        "Found member, but could not find DW_AT_variable_parameter attribute."));
  PL_ASSIGN_OR_RETURN(uint64_t val, AdaptLLVMOptional(attr.getAsUnsignedConstant(),
                                                      "Could not read attribute value."));

  return (val == 0x01);
}

// Returns the list of DIEs of the children of the input die and matches the filter_tag.
std::vector<DWARFDie> GetChildDIEs(const DWARFDie& die, llvm::dwarf::Tag filter_tag) {
  std::vector<DWARFDie> child_dies;
  for (const auto& die : die.children()) {
    if (die.getTag() != filter_tag) {
      continue;
    }
    child_dies.push_back(die);
  }
  return child_dies;
}

bool IsDeclaration(const llvm::DWARFDie& die) {
  auto value_or = GetDieAttribute(die, llvm::dwarf::DW_AT_declaration);
  if (value_or.ok()) {
    DCHECK(value_or.ValueOrDie().getForm() == llvm::dwarf::DW_FORM_flag_present)
        << "DW_AT_declaration should be of DW_FORM_flag_present. DIE: " << Dump(die);
  }
  return value_or.ok();
}

}  // namespace

StatusOr<uint64_t> DwarfReader::GetStructByteSize(std::string_view struct_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& struct_die,
                      GetMatchingDIE(struct_name, llvm::dwarf::DW_TAG_structure_type));

  return GetTypeByteSize(struct_die);
}

StatusOr<StructMemberInfo> DwarfReader::GetStructMemberInfo(std::string_view struct_name,
                                                            llvm::dwarf::Tag tag,
                                                            std::string_view member_name,
                                                            llvm::dwarf::Tag member_tag) {
  StructMemberInfo member_info;

  PL_ASSIGN_OR_RETURN(std::vector<DWARFDie> dies, GetMatchingDIEs(struct_name, {tag}));

  const DWARFDie* struct_def_die = nullptr;

  for (const auto& die : dies) {
    if (IsDeclaration(die)) {
      // Declaration DIE does not include the member DIEs.
      continue;
    }
    struct_def_die = &die;
  }

  if (struct_def_die == nullptr) {
    return error::NotFound("No definition of DIE found for name $0 and tag $1", struct_name,
                           magic_enum::enum_name(tag));
  }

  for (const auto& die : GetChildDIEs(*struct_def_die, member_tag)) {
    PL_ASSIGN_OR(std::string die_name, GetDieName(die), continue);

    if (die_name != member_name) {
      continue;
    }

    PL_ASSIGN_OR_RETURN(member_info.offset, GetMemberOffset(die));
    PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));
    PL_ASSIGN_OR_RETURN(member_info.type_info, GetTypeInfo(die, type_die));
    return member_info;
  }

  return error::Internal("Could not find member $0 in struct $1.", member_name, struct_name);
}

StatusOr<std::vector<StructSpecEntry>> DwarfReader::GetStructSpec(std::string_view struct_name) {
  StructMemberInfo member_info;

  PL_ASSIGN_OR_RETURN(const DWARFDie& struct_die,
                      GetMatchingDIE(struct_name, llvm::dwarf::DW_TAG_structure_type));

  std::vector<StructSpecEntry> output;
  // Start at the beginning (no prefix and offset 0).
  std::string path_prefix = "";
  int offset = 0;
  PL_RETURN_IF_ERROR(FlattenedStructSpec(struct_die, &output, path_prefix, offset));

  return output;
}

Status DwarfReader::FlattenedStructSpec(const llvm::DWARFDie& struct_die,
                                        std::vector<StructSpecEntry>* output,
                                        const std::string& path_prefix, int offset) {
  // We use the JSON pointer path separator, since the path is used to render the struct as JSON.
  // JSON pointer reference: https://tools.ietf.org/html/rfc6901.
  constexpr std::string_view kPathSep = "/";

  for (const auto& die : struct_die.children()) {
    if ((die.getTag() == llvm::dwarf::DW_TAG_member)) {
      std::string path = absl::StrCat(path_prefix, kPathSep, GetShortName(die));
      PL_ASSIGN_OR_RETURN(int member_offset, GetMemberOffset(die));

      PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));
      PL_ASSIGN_OR_RETURN(TypeInfo type_info, GetTypeInfo(die, type_die));

      if (type_info.type == VarType::kBaseType || type_info.type == VarType::kPointer) {
        PL_ASSIGN_OR_RETURN(int size, GetTypeByteSize(type_die));

        StructSpecEntry entry;
        entry.offset = offset + member_offset;
        entry.size = size;
        entry.type_info = type_info;
        entry.path = path;

        output->push_back(std::move(entry));
      } else {
        PL_RETURN_IF_ERROR(FlattenedStructSpec(type_die, output, path, offset + member_offset));
      }
    }
  }

  return Status::OK();
}

StatusOr<TypeInfo> DwarfReader::DereferencePointerType(std::string type_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& die,
                      GetMatchingDIE(type_name, llvm::dwarf::DW_TAG_pointer_type));
  PL_ASSIGN_OR_RETURN(const DWARFDie type_die, GetTypeDie(die));
  return GetTypeInfo(die, type_die);
}

StatusOr<uint64_t> DwarfReader::GetArgumentTypeByteSize(std::string_view function_symbol_name,
                                                        std::string_view arg_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  for (const auto& die : GetParamDIEs(function_die)) {
    if (GetShortName(die) == arg_name) {
      PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));
      return GetTypeByteSize(type_die);
    }
  }
  return error::Internal("Could not find argument.");
}

namespace {

// Get the DW_AT_location of a DIE as raw bytes.
// For DW_AT_location expressions that have different values for different address ranges,
// this function currently returns the value for the first address range (which should
// correspond to the location of the variable at the function entry).
StatusOr<llvm::ArrayRef<uint8_t>> GetDieLocationAttrBytes(const DWARFDie& die) {
  PL_ASSIGN_OR_RETURN(const DWARFFormValue& loc_attr,
                      AdaptLLVMOptional(die.find(llvm::dwarf::DW_AT_location),
                                        "Could not find DW_AT_location for function argument."));

  if (loc_attr.isFormClass(DWARFFormValue::FC_Block) ||
      loc_attr.isFormClass(DWARFFormValue::FC_Exprloc)) {
    PL_ASSIGN_OR_RETURN(llvm::ArrayRef<uint8_t> loc_block,
                        AdaptLLVMOptional(loc_attr.getAsBlock(), "Could not extract location."));

    return loc_block;
  }

  if (loc_attr.isFormClass(DWARFFormValue::FC_SectionOffset)) {
    PL_ASSIGN_OR_RETURN(uint64_t section_offset,
                        AdaptLLVMOptional(loc_attr.getAsSectionOffset(),
                                          "Could not extract location as section offset."));

    llvm::Expected<llvm::DWARFLocationExpressionsVector> location_expr_vec =
        die.getDwarfUnit()->findLoclistFromOffset(section_offset);

    std::error_code ec = errorToErrorCode(location_expr_vec.takeError());
    if (ec) {
      return error::Internal("Expected DWARFLocationExpressionsVector: $0", ec.message());
    }

    if (location_expr_vec->empty()) {
      return error::Internal("Emtpy DWARFLocationExpressionsVector");
    }

    // Now we have a vector of locations that look like the following:
    //   [0x000000000047f120, 0x000000000047f14d): DW_OP_reg0 RAX
    //   [0x000000000047f14d, 0x000000000047f1cd): DW_OP_call_frame_cfa)
    // Note that there is an instruction address range. Within that range of instructions,
    // we can expect to find the argument at the specified location.

    // For now, we use the first location, assuming that it is valid for the function entry.
    const llvm::DWARFLocationExpression& loc = location_expr_vec->front();
    VLOG(1) << to_string(loc);

    return llvm::ArrayRef<uint8_t>(loc.Expr);
  }

  return error::Internal("Unsupported Form: $0", magic_enum::enum_name(loc_attr.getForm()));
}

// Get the DW_AT_location of a DIE. Used for getting the location of variables,
// which may be either on the stack or in registers.
// Currently used on function arguments.
//
// Example:
//     0x00062106:   DW_TAG_formal_parameter
//                    DW_AT_name [DW_FORM_string] ("v")
//                    ...
//                    DW_AT_location [DW_FORM_block1] (DW_OP_call_frame_cfa)
// This example should return the location on the stack.
StatusOr<VarLocation> GetDieLocationAttr(const DWARFDie& die) {
  PL_ASSIGN_OR_RETURN(llvm::ArrayRef<uint8_t> loc_bytes, GetDieLocationAttrBytes(die));
  PL_ASSIGN_OR_RETURN(SimpleBlock loc_block, DecodeSimpleBlock(loc_bytes));

  if (loc_block.code == llvm::dwarf::LocationAtom::DW_OP_fbreg) {
    if (loc_block.operand >= 0) {
      return VarLocation{.loc_type = LocationType::kStack, .offset = loc_block.operand};
    } else {
      // TODO(oazizi): Hacky code used for CPP DWARF info. Needs to be re-written.
      loc_block.operand *= -1;
      return VarLocation{.loc_type = LocationType::kRegister, .offset = loc_block.operand};
    }
  }

  if (loc_block.code == llvm::dwarf::LocationAtom::DW_OP_call_frame_cfa) {
    // DW_OP_call_frame_cfa is observed in golang dwarf symbols.
    // This logic is probably not right, but appears to work for now.
    // TODO(oazizi): Study call_frame_cfa blocks.
    return VarLocation{.loc_type = LocationType::kStack, .offset = 0};
  }

  if (loc_block.code >= llvm::dwarf::LocationAtom::DW_OP_reg0 &&
      loc_block.code <= llvm::dwarf::LocationAtom::DW_OP_reg31) {
    int reg_num = loc_block.code - llvm::dwarf::LocationAtom::DW_OP_reg0;
    return VarLocation{.loc_type = LocationType::kRegister, .offset = reg_num};
  }

  return error::Unimplemented("Unsupported code (location atom): $0", loc_block.code);
}

}  // namespace

StatusOr<VarLocation> DwarfReader::GetArgumentLocation(std::string_view function_symbol_name,
                                                       std::string_view arg_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  for (const auto& die : GetParamDIEs(function_die)) {
    if (die.getName(llvm::DINameKind::ShortName) == arg_name) {
      return GetDieLocationAttr(die);
    }
  }
  return error::Internal("Could not find argument.");
}

namespace {

ABI LanguageToABI(llvm::dwarf::SourceLanguage lang) {
  switch (lang) {
    case llvm::dwarf::DW_LANG_Go:
      // No arguments are passed through register on Go.
      // TODO(oazizi): This should be expanded to cover Go's new ABI depending on version.
      return ABI::kGolangStack;
    case llvm::dwarf::DW_LANG_C:
    case llvm::dwarf::DW_LANG_C_plus_plus:
    case llvm::dwarf::DW_LANG_C_plus_plus_03:
    case llvm::dwarf::DW_LANG_C_plus_plus_11:
    case llvm::dwarf::DW_LANG_C_plus_plus_14:
      // TODO(oazizi): We assume AMD64 for now, but need to support other architectures.
      return ABI::kSystemVAMD64;
    default:
      return ABI::kUnknown;
  }
}
}  // namespace

StatusOr<std::map<std::string, ArgInfo>> DwarfReader::GetFunctionArgInfo(
    std::string_view function_symbol_name) {
  std::map<std::string, ArgInfo> arg_info;

  // Ideally, we'd use DW_AT_location directly from DWARF, (via GetDieLocationAttr(die),
  // but DW_AT_location has been found to be blank in some cases, making it unreliable.
  // Instead, we use a FunctionArgTracker that tries to reverse engineer the calling convention.

  ABI abi = LanguageToABI(source_language_);
  if (abi == ABI::kUnknown) {
    return error::Unimplemented("Unable to determine ABI from language: $0",
                                magic_enum::enum_name(source_language_));
  }
  FunctionArgTracker arg_tracker(abi);

  // If return value is not able to be passed in as a register,
  // then the first argument register becomes a pointer to the return value.
  // Account for that here.
  PL_ASSIGN_OR_RETURN(RetValInfo ret_val_info, GetFunctionRetValInfo(function_symbol_name));
  arg_tracker.AdjustForReturnValue(ret_val_info.byte_size);

  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  for (const auto& die : GetParamDIEs(function_die)) {
    VLOG(1) << die.getName(llvm::DINameKind::ShortName);
    auto& arg = arg_info[die.getName(llvm::DINameKind::ShortName)];

    PL_ASSIGN_OR_RETURN(const DWARFDie type_die, GetTypeDie(die));
    PL_ASSIGN_OR_RETURN(arg.type_info, GetTypeInfo(die, type_die));

    PL_ASSIGN_OR_RETURN(uint64_t type_size, GetTypeByteSize(type_die));
    PL_ASSIGN_OR_RETURN(uint64_t alignment_size, GetAlignmentByteSize(type_die));
    PL_ASSIGN_OR_RETURN(arg.location, arg_tracker.PopLocation(type_size, alignment_size));

    if (source_language_ == llvm::dwarf::DW_LANG_Go) {
      arg.retarg = IsGolangRetArg(die).ValueOr(false);
    }
  }

  return arg_info;
}

StatusOr<RetValInfo> DwarfReader::GetFunctionRetValInfo(std::string_view function_symbol_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  if (!function_die.find(llvm::dwarf::DW_AT_type).hasValue()) {
    // No return type means the function has a void return type.
    return RetValInfo{.type_info = TypeInfo{.type = VarType::kVoid, .type_name = ""},
                      .byte_size = 0};
  }

  RetValInfo ret_val_info;

  PL_ASSIGN_OR_RETURN(const DWARFDie type_die, GetTypeDie(function_die));
  PL_ASSIGN_OR_RETURN(ret_val_info.type_info, GetTypeInfo(function_die, type_die));
  PL_ASSIGN_OR_RETURN(ret_val_info.byte_size, GetTypeByteSize(type_die));

  return ret_val_info;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
