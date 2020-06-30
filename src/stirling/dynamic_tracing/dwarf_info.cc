#include "src/stirling/dynamic_tracing/dwarf_info.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/obj_tools/dwarf_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using dwarf_tools::ArgInfo;
using dwarf_tools::VarInfo;
using dwarf_tools::VarType;

// Map to convert Go Base types to ScalarType.
// clang-format off
const std::map<std::string_view, ir::shared::ScalarType> kGoTypesMap = {
        {"bool", ir::shared::ScalarType::BOOL},
        {"int", ir::shared::ScalarType::INT},
        {"int8", ir::shared::ScalarType::INT8},
        {"int16", ir::shared::ScalarType::INT16},
        {"int32", ir::shared::ScalarType::INT32},
        {"int64", ir::shared::ScalarType::INT64},
        {"uint", ir::shared::ScalarType::UINT},
        {"uint8", ir::shared::ScalarType::UINT8},
        {"uint16", ir::shared::ScalarType::UINT16},
        {"uint32", ir::shared::ScalarType::UINT32},
        {"uint64", ir::shared::ScalarType::UINT64},
        {"float32", ir::shared::ScalarType::FLOAT},
        {"float64", ir::shared::ScalarType::DOUBLE},
};
// clang-format on

StatusOr<ir::shared::ScalarType> VarTypeToProtoScalarType(const VarType& type,
                                                          std::string_view name) {
  switch (type) {
    case VarType::kBaseType: {
      auto iter = kGoTypesMap.find(name);
      if (iter == kGoTypesMap.end()) {
        return error::Internal("Unrecognized base type: $0", name);
      }
      return iter->second;
    }
    case VarType::kPointer:
      return ir::shared::ScalarType::VOID_POINTER;
    default:
      return error::Internal("Unhandled type: $0", magic_enum::enum_name(type));
  }
}

StatusOr<const ArgInfo*> GetArgInfo(const std::map<std::string, ArgInfo>& args_map,
                                    std::string_view arg_name) {
  auto args_map_iter = args_map.find(std::string(arg_name));
  if (args_map_iter == args_map.end()) {
    return error::Internal("Could not find argument $0", arg_name);
  }
  return &args_map_iter->second;
}

StatusOr<ir::physical::PhysicalProbe> AddDwarves(const ir::logical::Probe& input_probe) {
  using dwarf_tools::DwarfReader;

  const std::string& binary_path = input_probe.trace_point().binary_path();
  const std::string& function_symbol = input_probe.trace_point().symbol();

  PL_ASSIGN_OR_RETURN(std::unique_ptr<DwarfReader> dwarf_reader, DwarfReader::Create(binary_path));

  PL_ASSIGN_OR_RETURN(auto args_map, dwarf_reader->GetFunctionArgInfo(function_symbol));

  ir::physical::PhysicalProbe out;

  out.mutable_trace_point()->CopyFrom(input_probe.trace_point());

  // Add SP variable.
  {
    auto* var = out.add_vars();
    var->set_name("sp");
    var->set_type(ir::shared::VOID_POINTER);
    var->set_reg(ir::physical::Register::SP);
  }

  // Dwarf and BCC have an 8 byte difference in where they believe the SP is.
  // This adjustment factor accounts for that difference.
  constexpr int32_t kSPOffset = 8;

  for (auto& arg : input_probe.args()) {
    // For now, just assume the expression is a simple arg name.
    // TODO(oazizi): Support expressions.
    std::string arg_name = arg.expr();

    std::vector<std::string_view> components = absl::StrSplit(arg_name, ".");

    PL_ASSIGN_OR_RETURN(const ArgInfo* arg_info, GetArgInfo(args_map, components.front()));
    DCHECK(arg_info != nullptr);

    VarType type = arg_info->type;
    std::string type_name = std::move(arg_info->type_name);
    int offset = kSPOffset + arg_info->offset;

    for (auto iter = components.begin() + 1; iter < components.end(); ++iter) {
      std::string_view field_name = *iter;

      PL_ASSIGN_OR_RETURN(VarInfo member_info,
                          dwarf_reader->GetStructMemberInfo(type_name, field_name));
      offset += member_info.offset;
      type_name = std::move(member_info.type_name);
      type = member_info.type;
    }

    PL_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type, VarTypeToProtoScalarType(type, type_name));

    auto* var = out.add_vars();
    var->set_name(arg.id());
    var->set_type(pb_type);
    var->mutable_memory()->set_base("sp");
    var->mutable_memory()->set_offset(offset);
  }

  for (auto& ret_val : input_probe.ret_vals()) {
    // TODO(oazizi): Support named return variables.
    // Golang automatically names return variables ~r0, ~r1, etc.
    // However, it should be noted that the indexing includes function arguments.
    // For example Foo(a int, b int) (int, int) would have ~r2 and ~r3 as return variables.
    // One additional nuance is that the receiver, although an argument for dwarf purposes,
    // is not counted in the indexing.
    // For now, we throw the burden of finding the index to the user,
    // so if they want the first return argument above, they would have to specify and index of 2.
    // TODO(oazizi): Make indexing of return value based on number of return arguments only.
    std::string ret_val_name = absl::StrCat("~r", std::to_string(ret_val.index()));

    PL_ASSIGN_OR_RETURN(const ArgInfo* arg_info, GetArgInfo(args_map, ret_val_name));
    DCHECK(arg_info != nullptr);

    PL_ASSIGN_OR_RETURN(ir::shared::ScalarType type,
                        VarTypeToProtoScalarType(arg_info->type, arg_info->type_name));

    auto* var = out.add_vars();
    var->set_name(ret_val.id());
    var->set_type(type);
    var->mutable_memory()->set_base("sp");
    var->mutable_memory()->set_offset(arg_info->offset + 8);
  }

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
