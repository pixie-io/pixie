#include "src/stirling/obj_tools/dwarf_tools.h"

#include <llvm/DebugInfo/DIContext.h>
#include <llvm/Object/ObjectFile.h>

namespace pl {
namespace stirling {
namespace dwarf_tools {

using llvm::DWARFContext;
using llvm::DWARFDie;

StatusOr<std::unique_ptr<DwarfReader>> DwarfReader::Create(std::string_view obj_filename) {
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

  return std::unique_ptr<DwarfReader>(
      new DwarfReader(std::move(buffer), DWARFContext::create(*obj_file)));
}

namespace {

bool IsMatchingDIE(std::string_view name, llvm::dwarf::Tag tag, std::string_view die_name,
                   llvm::dwarf::Tag die_tag) {
  return (tag == static_cast<llvm::dwarf::Tag>(llvm::dwarf::DW_TAG_invalid) || (tag == die_tag)) &&
         (name == die_name);
}

}  // namespace

Status DwarfReader::GetMatchingDIEs(DWARFContext::unit_iterator_range CUs, std::string_view name,
                                    llvm::dwarf::Tag tag, std::vector<DWARFDie>* dies_out) {
  for (const auto& CU : CUs) {
    for (const auto& Entry : CU->dies()) {
      DWARFDie die = {CU.get(), &Entry};
      if (const char* die_name = die.getName(llvm::DINameKind::ShortName)) {
        if (IsMatchingDIE(name, tag, die_name, die.getTag())) {
          dies_out->push_back(std::move(die));
          continue;
        }
      }
      if (const char* die_name = die.getName(llvm::DINameKind::LinkageName)) {
        if (IsMatchingDIE(name, tag, die_name, die.getTag())) {
          dies_out->push_back(std::move(die));
        }
      }
    }
  }

  return Status::OK();
}

StatusOr<std::vector<DWARFDie>> DwarfReader::GetMatchingDIEs(std::string_view name,
                                                             llvm::dwarf::Tag type) {
  DCHECK(dwarf_context_ != nullptr);
  std::vector<DWARFDie> dies;

  PL_RETURN_IF_ERROR(GetMatchingDIEs(dwarf_context_->normal_units(), name, type, &dies));
  // TODO(oazizi): Might want to consider dwarf_context_->dwo_units() as well.

  return dies;
}

StatusOr<int> DwarfReader::GetStructMemberOffset(std::string_view struct_name,
                                                 std::string member_name) {
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

  return error::Internal("Could not find member");
}

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
