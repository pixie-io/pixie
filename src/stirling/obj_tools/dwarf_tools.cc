#include "src/stirling/obj_tools/dwarf_tools.h"

#include <algorithm>

#include <llvm/DebugInfo/DIContext.h>
#include <llvm/Object/ObjectFile.h>

#include "src/stirling/obj_tools/init.h"

#define LLVM_ASSIGN_OR_RETURN_IMPL(var, tmpvar, expr, err) \
  auto tmpvar = expr;                                      \
  if (!tmpvar.hasValue()) {                                \
    return error::Internal(err);                           \
  }                                                        \
  var = tmpvar.getValue();

#define LLVM_ASSIGN_OR_RETURN(var, expr, err) \
  LLVM_ASSIGN_OR_RETURN_IMPL(var, PL_CONCAT_NAME(__t__, __COUNTER__), expr, err)

namespace pl {
namespace stirling {
namespace dwarf_tools {

using llvm::DWARFContext;
using llvm::DWARFDie;
using llvm::DWARFFormValue;

// TODO(oazizi): This will break on 32-bit binaries.
// Use ELF to get the correct value.
// https://superuser.com/questions/791506/how-to-determine-if-a-linux-binary-file-is-32-bit-or-64-bit
uint8_t kAddressSize = sizeof(void*);

StatusOr<std::unique_ptr<DwarfReader>> DwarfReader::Create(std::string_view obj_filename,
                                                           bool index) {
  using llvm::MemoryBuffer;

  std::error_code ec;

  llvm::ErrorOr<std::unique_ptr<MemoryBuffer>> buff_or_err =
      MemoryBuffer::getFileOrSTDIN(std::string(obj_filename));
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
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_subprogram:
      return true;
    default:
      return false;
  }
}
}  // namespace

void DwarfReader::IndexDIEs() {
  DWARFContext::unit_iterator_range CUs = dwarf_context_->normal_units();

  for (const auto& CU : CUs) {
    for (const auto& Entry : CU->dies()) {
      DWARFDie die = {CU.get(), &Entry};
      llvm::dwarf::Tag tag = die.getTag();

      if (IsIndexedType(tag)) {
        const char* die_short_name = die.getName(llvm::DINameKind::ShortName);
        if (die_short_name != nullptr) {
          auto& die_type_map = die_map_[tag];

          // TODO(oazizi): What's the right way to deal with duplicate names?
          // Only appears to happen with structs like the following:
          //  ThreadStart, _IO_FILE, _IO_marker, G, in6_addr
          // So probably okay for now. But need to be wary of this.
          if (die_type_map.find(die_short_name) != die_type_map.end()) {
            VLOG(1) << "Duplicate name: " << die_short_name;
          }
          die_type_map[die_short_name] = die;
        }
      }
    }
  }
}

StatusOr<std::vector<DWARFDie>> DwarfReader::GetMatchingDIEs(std::string_view name,
                                                             std::optional<llvm::dwarf::Tag> type) {
  DCHECK(dwarf_context_ != nullptr);
  std::vector<DWARFDie> dies;

  // Special case for types that are indexed (currently only struct types);
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
  // TODO(oazizi): Might want to consider dwarf_context_->dwo_units() as well.

  return dies;
}

StatusOr<DWARFDie> DwarfReader::GetMatchingDIE(std::string_view name,
                                               std::optional<llvm::dwarf::Tag> type) {
  PL_ASSIGN_OR_RETURN(std::vector<DWARFDie> dies, GetMatchingDIEs(name, type));
  if (dies.empty()) {
    return error::Internal("Could not locate symbol name.");
  }
  if (dies.size() > 1) {
    return error::Internal("Found too many DIE matches.");
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
  llvm::DWARFExpression expr(data, llvm::DWARFExpression::Operation::Dwarf4, kAddressSize);

  auto iter = expr.begin();
  auto& operation = *iter;
  ++iter;
  if (iter != expr.end()) {
    return error::Internal("Operation include more than one component. Not yet supported.");
  }

  SimpleBlock decoded_block;

  decoded_block.code = static_cast<llvm::dwarf::LocationAtom>(operation.getCode());
  decoded_block.operand = operation.getRawOperand(0);
  VLOG(1) << absl::Substitute("Decoded block: code=$0 operand=$1",
                              magic_enum::enum_name(decoded_block.code), decoded_block.operand);

  return decoded_block;
}

StatusOr<uint64_t> GetMemberOffset(const DWARFDie& die) {
  DCHECK(die.getTag() == llvm::dwarf::DW_TAG_member);

  const char* die_short_name = die.getName(llvm::DINameKind::ShortName);

  LLVM_ASSIGN_OR_RETURN(DWARFFormValue & attr, die.find(llvm::dwarf::DW_AT_data_member_location),
                        "Found member, but could not find data_member_location attribute.");

  // We've run into a case where the offset is encoded as a block instead of a constant.
  // See section 7.5.5 of the spec: http://dwarfstd.org/doc/DWARF5.pdf
  // Or use llvm-dwarfdump on testdata/sockshop_payments_service to see an example.
  // This handles that case as long as the block is simple (which is what we've seen in practice).
  if (attr.getForm() == llvm::dwarf::DW_FORM_block1) {
    LLVM_ASSIGN_OR_RETURN(llvm::ArrayRef<uint8_t> offset_block, attr.getAsBlock(),
                          "Could not extract block.");

    PL_ASSIGN_OR_RETURN(SimpleBlock decoded_offset_block, DecodeSimpleBlock(offset_block));

    if (decoded_offset_block.code == llvm::dwarf::LocationAtom::DW_OP_plus_uconst) {
      return decoded_offset_block.operand;
    }

    return error::Internal("Unsupported operand: $0",
                           magic_enum::enum_name(decoded_offset_block.code));
  }

  LLVM_ASSIGN_OR_RETURN(
      uint64_t offset, attr.getAsUnsignedConstant(),
      absl::Substitute("Could not extract offset for member $0.", die_short_name));
  return offset;
}

StatusOr<DWARFDie> GetTypeDie(const DWARFDie& die) {
  DWARFDie type_die = die;
  do {
    LLVM_ASSIGN_OR_RETURN(DWARFFormValue & type_attr, type_die.find(llvm::dwarf::DW_AT_type),
                          "Could not find DW_AT_type.");

    type_die = die.getAttributeValueAsReferencedDie(type_attr);
  } while (type_die.getTag() == llvm::dwarf::DW_TAG_typedef);

  return type_die;
}

StatusOr<uint64_t> GetBaseOrStructTypeByteSize(const DWARFDie& die) {
  DCHECK((die.getTag() == llvm::dwarf::DW_TAG_base_type) ||
         (die.getTag() == llvm::dwarf::DW_TAG_structure_type));

  LLVM_ASSIGN_OR_RETURN(DWARFFormValue & byte_size_attr, die.find(llvm::dwarf::DW_AT_byte_size),
                        "Could not find DW_AT_byte_size.");

  LLVM_ASSIGN_OR_RETURN(uint64_t byte_size, byte_size_attr.getAsUnsignedConstant(),
                        "Could not extract byte_size.");

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
          absl::Substitute("Unexpected DIE type: $0", magic_enum::enum_name(die.getTag())));
  }
}

StatusOr<VarType> GetType(const DWARFDie& die) {
  DCHECK(die.isValid());

  switch (die.getTag()) {
    case llvm::dwarf::DW_TAG_pointer_type:
      return VarType::kPointer;
    case llvm::dwarf::DW_TAG_subroutine_type:
      return VarType::kSubroutine;
    case llvm::dwarf::DW_TAG_base_type:
      return VarType::kBaseType;
    case llvm::dwarf::DW_TAG_structure_type:
      return VarType::kStruct;
    default:
      return error::Internal(
          absl::Substitute("Unexpected DIE type: $0", magic_enum::enum_name(die.getTag())));
  }
}

StatusOr<std::string> GetTypeName(const DWARFDie& die) {
  DCHECK(die.isValid());

  switch (die.getTag()) {
    case llvm::dwarf::DW_TAG_subroutine_type:
      return std::string("func");
    case llvm::dwarf::DW_TAG_pointer_type: {
      PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));
      return std::string(type_die.getName(llvm::DINameKind::ShortName));
    }
    case llvm::dwarf::DW_TAG_base_type:
    case llvm::dwarf::DW_TAG_structure_type:
      return std::string(die.getName(llvm::DINameKind::ShortName));
    default:
      return error::Internal(
          absl::Substitute("Unexpected DIE type: $0", magic_enum::enum_name(die.getTag())));
  }
}

// This function calls out any die with DW_AT_variable_parameter == 0x1 to be a return value..
// The documentation on how Golang sets this field is sparse, so not sure if this is the
// right flag to look at.
// From examining a few cases, this interpretation of the flag seems to work, but it might
// also be completely wrong.
// TODO(oazizi): Find a better way to determine return values.
StatusOr<bool> IsGolangRetArg(const DWARFDie& die) {
  LLVM_ASSIGN_OR_RETURN(DWARFFormValue & attr, die.find(llvm::dwarf::DW_AT_variable_parameter),
                        "Found member, but could not find DW_AT_variable_parameter attribute.");
  LLVM_ASSIGN_OR_RETURN(uint64_t val, attr.getAsUnsignedConstant(),
                        "Could not read attribute value.");

  return (val == 0x01);
}

}  // namespace

StatusOr<VarInfo> DwarfReader::GetStructMemberInfo(std::string_view struct_name,
                                                   std::string_view member_name) {
  VarInfo member_info;

  PL_ASSIGN_OR_RETURN(const DWARFDie& struct_die,
                      GetMatchingDIE(struct_name, llvm::dwarf::DW_TAG_structure_type));

  // TODO(oazizi): This pattern repeats. Create a helper function to find the child.
  for (const auto& die : struct_die.children()) {
    if ((die.getTag() == llvm::dwarf::DW_TAG_member) &&
        (die.getName(llvm::DINameKind::ShortName) == member_name)) {
      PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));

      PL_ASSIGN_OR_RETURN(member_info.offset, GetMemberOffset(die));

      PL_ASSIGN_OR_RETURN(member_info.type, GetType(type_die));
      PL_ASSIGN_OR_RETURN(member_info.type_name, GetTypeName(type_die));
      return member_info;
    }
  }

  return error::Internal("Could not find member.");
}

StatusOr<uint64_t> DwarfReader::GetArgumentTypeByteSize(std::string_view function_symbol_name,
                                                        std::string_view arg_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  for (const auto& die : function_die.children()) {
    if ((die.getTag() == llvm::dwarf::DW_TAG_formal_parameter) &&
        (die.getName(llvm::DINameKind::ShortName) == arg_name)) {
      PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));
      return GetTypeByteSize(type_die);
    }
  }
  return error::Internal("Could not find argument.");
}

StatusOr<int64_t> DwarfReader::GetArgumentStackPointerOffset(std::string_view function_symbol_name,
                                                             std::string_view arg_name) {
  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  for (const auto& die : function_die.children()) {
    if ((die.getTag() == llvm::dwarf::DW_TAG_formal_parameter) &&
        (die.getName(llvm::DINameKind::ShortName) == arg_name)) {
      LLVM_ASSIGN_OR_RETURN(DWARFFormValue & loc_attr, die.find(llvm::dwarf::DW_AT_location),
                            "Could not find DW_AT_location for function argument.");

      if (!loc_attr.isFormClass(DWARFFormValue::FC_Block) &&
          !loc_attr.isFormClass(DWARFFormValue::FC_Exprloc)) {
        return error::Internal("Unexpected Form: $0", magic_enum::enum_name(loc_attr.getForm()));
      }

      LLVM_ASSIGN_OR_RETURN(llvm::ArrayRef<uint8_t> loc_block, loc_attr.getAsBlock(),
                            "Could not extract location.");

      PL_ASSIGN_OR_RETURN(SimpleBlock decoded_loc_block, DecodeSimpleBlock(loc_block));

      if (decoded_loc_block.code == llvm::dwarf::LocationAtom::DW_OP_fbreg) {
        return decoded_loc_block.operand;
      }
      if (decoded_loc_block.code == llvm::dwarf::LocationAtom::DW_OP_call_frame_cfa) {
        return 0;
      }

      return error::Internal("Unsupported operand: $0",
                             magic_enum::enum_name(decoded_loc_block.code));
    }
  }
  return error::Internal("Could not find argument.");
}

namespace {
// This function takes an address, and if it is not a multiple of the `size` parameter,
// it rounds it up to so that it is aligned to the given `size`.
// Examples:
//   Align(64, 8) = 64
//   Align(66, 8) = 70
uint64_t Align(uint64_t addr, uint64_t size) {
  uint64_t remainder = addr % size;
  return (remainder == 0) ? addr : (addr + size - remainder);
}
}  // namespace

StatusOr<std::map<std::string, ArgInfo>> DwarfReader::GetFunctionArgInfo(
    std::string_view function_symbol_name) {
  std::map<std::string, ArgInfo> arg_info;
  uint64_t current_offset = 0;

  PL_ASSIGN_OR_RETURN(const DWARFDie& function_die,
                      GetMatchingDIE(function_symbol_name, llvm::dwarf::DW_TAG_subprogram));

  for (const auto& die : function_die.children()) {
    if (die.getTag() == llvm::dwarf::DW_TAG_formal_parameter) {
      VLOG(1) << die.getName(llvm::DINameKind::ShortName);
      auto& arg = arg_info[die.getName(llvm::DINameKind::ShortName)];

      PL_ASSIGN_OR_RETURN(DWARFDie type_die, GetTypeDie(die));

      PL_ASSIGN_OR_RETURN(uint64_t type_size, GetTypeByteSize(type_die));
      PL_ASSIGN_OR_RETURN(uint64_t alignment_size, GetAlignmentByteSize(type_die));

      current_offset = Align(current_offset, alignment_size);
      arg.offset = current_offset;
      current_offset += type_size;

      PL_ASSIGN_OR_RETURN(arg.type, GetType(type_die));
      PL_ASSIGN_OR_RETURN(arg.type_name, GetTypeName(type_die));

      // TODO(oazizi): This is specific for Golang.
      //               Put into if statement once we know what language we are analyzing.
      PL_ASSIGN_OR(arg.retarg, IsGolangRetArg(die), __s__ = false;);
    }
  }

  return arg_info;
}

}  // namespace dwarf_tools
}  // namespace stirling
}  // namespace pl
