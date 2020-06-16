#include "src/stirling/obj_tools/dwarf_tools.h"

#include <llvm/DebugInfo/DIContext.h>
#include <llvm/Object/ObjectFile.h>

#include "src/stirling/obj_tools/init.h"

namespace pl {
namespace stirling {
namespace dwarf_tools {

using llvm::DWARFContext;
using llvm::DWARFDie;

StatusOr<std::unique_ptr<DwarfReader>> DwarfReader::Create(std::string_view obj_filename,
                                                           bool index) {
  using llvm::MemoryBuffer;

  std::error_code ec;

  llvm::ErrorOr<std::unique_ptr<MemoryBuffer>> buff_or_err =
      MemoryBuffer::getFileOrSTDIN(std::string(obj_filename));
  ec = buff_or_err.getError();
  if (ec) {
    return error::Internal(ec.message());
  }

  std::unique_ptr<llvm::MemoryBuffer> buffer = std::move(buff_or_err.get());
  llvm::Expected<std::unique_ptr<llvm::object::Binary>> bin_or_err =
      llvm::object::createBinary(*buffer);
  ec = errorToErrorCode(bin_or_err.takeError());
  if (ec) {
    return error::Internal(ec.message());
  }

  auto* obj_file = llvm::dyn_cast<llvm::object::ObjectFile>(bin_or_err->get());
  if (!obj_file) {
    return error::Internal("Could not create DWARFContext.");
  }

  auto dwarf_reader = std::unique_ptr<DwarfReader>(
      new DwarfReader(std::move(buffer), DWARFContext::create(*obj_file)));
  if (index) {
    dwarf_reader->IndexStructs();
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

bool IsMatchingTag(llvm::dwarf::Tag tag, const DWARFDie& die) {
  llvm::dwarf::Tag die_tag = die.getTag();
  return (tag == static_cast<llvm::dwarf::Tag>(llvm::dwarf::DW_TAG_invalid) || (tag == die_tag));
}

bool IsMatchingDIE(std::string_view name, llvm::dwarf::Tag tag, const DWARFDie& die) {
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
                                    llvm::dwarf::Tag tag, std::vector<DWARFDie>* dies_out) {
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

void DwarfReader::IndexStructs() {
  // For now, we only index structure types.
  // TODO(oazizi): Expand to cover other types, when needed.
  llvm::dwarf::Tag tag = llvm::dwarf::DW_TAG_structure_type;

  DWARFContext::unit_iterator_range CUs = dwarf_context_->normal_units();

  for (const auto& CU : CUs) {
    for (const auto& Entry : CU->dies()) {
      DWARFDie die = {CU.get(), &Entry};
      if (IsMatchingTag(tag, die)) {
        const char* die_short_name = die.getName(llvm::DINameKind::ShortName);
        if (die_short_name != nullptr) {
          // TODO(oazizi): What's the right way to deal with duplicate names?
          // Only appears to happen with structs like the following:
          //  ThreadStart, _IO_FILE, _IO_marker, G, in6_addr
          // So probably okay for now. But need to be wary of this.
          if (die_struct_map_.find(die_short_name) != die_struct_map_.end()) {
            VLOG(1) << "Duplicate name: " << die_short_name;
          }
          die_struct_map_[die_short_name] = die;
        }
      }
    }
  }
}

StatusOr<std::vector<DWARFDie>> DwarfReader::GetMatchingDIEs(std::string_view name,
                                                             llvm::dwarf::Tag type) {
  DCHECK(dwarf_context_ != nullptr);
  std::vector<DWARFDie> dies;

  // Special case for types that are indexed (currently only struct types);
  if (type == llvm::dwarf::DW_TAG_structure_type && !die_struct_map_.empty()) {
    auto iter = die_struct_map_.find(name);
    if (iter != die_struct_map_.end()) {
      return std::vector<DWARFDie>{iter->second};
    }
    return {};
  }

  PL_RETURN_IF_ERROR(GetMatchingDIEs(dwarf_context_->normal_units(), name, type, &dies));
  // TODO(oazizi): Might want to consider dwarf_context_->dwo_units() as well.

  return dies;
}

StatusOr<uint64_t> DwarfReader::GetStructMemberOffset(std::string_view struct_name,
                                                      std::string_view member_name) {
  PL_ASSIGN_OR_RETURN(std::vector<DWARFDie> dies,
                      GetMatchingDIEs(struct_name, llvm::dwarf::DW_TAG_structure_type));
  if (dies.empty()) {
    return error::Internal("Could not locate structure");
  }
  if (dies.size() > 1) {
    return error::Internal("Found too many DIE matches");
  }

  DWARFDie& struct_die = dies.front();

  for (const auto& die : struct_die.children()) {
    if ((die.getTag() == llvm::dwarf::DW_TAG_member) &&
        (die.getName(llvm::DINameKind::ShortName) == member_name)) {
      llvm::Optional<llvm::DWARFFormValue> attr = die.find(llvm::dwarf::DW_AT_data_member_location);
      if (!attr.hasValue()) {
        return error::Internal("Found member, but could not find data_member_location attribute.");
      }
      llvm::Optional<uint64_t> offset = attr.getValue().getAsUnsignedConstant();
      if (!offset.hasValue()) {
        return error::Internal("Could not extract offset.");
      }
      return offset.getValue();
    }
  }

  return error::Internal("Could not find member.");
}

namespace {
StatusOr<uint64_t> GetTypeByteSize(const DWARFDie& die) {
  if (die.getTag() == llvm::dwarf::DW_TAG_pointer_type) {
    // TODO(oazizi): This will break on 32-bit binary.
    // Use ELF to get the correct value.
    // https://superuser.com/questions/791506/how-to-determine-if-a-linux-binary-file-is-32-bit-or-64-bit
    return sizeof(void*);
  }

  if ((die.getTag() != llvm::dwarf::DW_TAG_base_type) &&
      (die.getTag() != llvm::dwarf::DW_TAG_structure_type)) {
    return error::Internal(
        absl::Substitute("Unexpected DIE type: $0", magic_enum::enum_name(die.getTag())));
  }

  llvm::Optional<llvm::DWARFFormValue> byte_size_attr = die.find(llvm::dwarf::DW_AT_byte_size);
  if (!byte_size_attr.hasValue()) {
    return error::Internal("Could not find DW_AT_byte_size.");
  }

  llvm::Optional<uint64_t> byte_size = byte_size_attr.getValue().getAsUnsignedConstant();
  if (!byte_size.hasValue()) {
    return error::Internal("Could not extract byte_size.");
  }

  return byte_size.getValue();
}
}  // namespace

StatusOr<uint64_t> DwarfReader::GetArgumentTypeByteSize(std::string_view symbol_name,
                                                        std::string_view arg_name) {
  PL_ASSIGN_OR_RETURN(std::vector<DWARFDie> dies,
                      GetMatchingDIEs(symbol_name, llvm::dwarf::DW_TAG_subprogram));
  if (dies.empty()) {
    return error::Internal("Could not locate structure");
  }
  if (dies.size() > 1) {
    return error::Internal("Found too many DIE matches");
  }

  DWARFDie& function_die = dies.front();

  for (const auto& die : function_die.children()) {
    if ((die.getTag() == llvm::dwarf::DW_TAG_formal_parameter) &&
        (die.getName(llvm::DINameKind::ShortName) == arg_name)) {
      llvm::Optional<llvm::DWARFFormValue> type_attr = die.find(llvm::dwarf::DW_AT_type);
      if (!type_attr.hasValue()) {
        return error::Internal("Found argument, but could not determine its type.");
      }
      llvm::Optional<uint64_t> type = type_attr.getValue().getAsReference();
      if (!type.hasValue()) {
        return error::Internal("Could not extract type.");
      }

      return GetTypeByteSize(die.getAttributeValueAsReferencedDie(type_attr.getValue()));

      // TODO(oazizi): Extract location information directly, using something like the following:
      //
      // llvm::Optional<llvm::DWARFFormValue> loc_attr = die.find(llvm::dwarf::DW_AT_location);
      // if (!loc_attr.hasValue()) {
      //   return error::Internal("Found argument, but could not determine its location.");
      // }
      //
      // LOG(INFO) << magic_enum::enum_name(loc_attr.getValue().getForm());
      // llvm::Optional<uint64_t> loc = loc_attr.getValue().getAsReference();
      // if (!loc.hasValue()) {
      //   return error::Internal("Could not extract location.");
      // }
      //
      // loc.getValue()
    }
  }
  return error::Internal("Could not find argument.");
}

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
