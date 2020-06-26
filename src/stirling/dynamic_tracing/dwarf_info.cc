#include "src/stirling/dynamic_tracing/dwarf_info.h"

#include <map>
#include <memory>
#include <string>

#include "src/stirling/obj_tools/dwarf_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using dwarf_tools::ArgInfo;
using dwarf_tools::ArgType;

// TODO(oazizi): Consider using dynamictracingpb::ScalarType directly in ArgInfo,
//               to avoid this additional mapping.
const std::map<ArgType, dynamictracingpb::ScalarType> kArgTypeToProtoScalarType = {
    {ArgType::kBool, dynamictracingpb::ScalarType::BOOL},
    {ArgType::kInt, dynamictracingpb::ScalarType::INT},
    {ArgType::kInt8, dynamictracingpb::ScalarType::INT8},
    {ArgType::kInt16, dynamictracingpb::ScalarType::INT16},
    {ArgType::kInt32, dynamictracingpb::ScalarType::INT32},
    {ArgType::kInt64, dynamictracingpb::ScalarType::INT64},
    {ArgType::kUInt, dynamictracingpb::ScalarType::UINT},
    {ArgType::kUInt8, dynamictracingpb::ScalarType::UINT8},
    {ArgType::kUInt16, dynamictracingpb::ScalarType::UINT16},
    {ArgType::kUInt32, dynamictracingpb::ScalarType::UINT32},
    {ArgType::kUInt64, dynamictracingpb::ScalarType::UINT64},
    {ArgType::kFloat32, dynamictracingpb::ScalarType::FLOAT},
    {ArgType::kFloat64, dynamictracingpb::ScalarType::DOUBLE},
    {ArgType::kPointer, dynamictracingpb::ScalarType::VOID_POINTER},
};

StatusOr<dynamictracingpb::ScalarType> ArgTypeToProtoScalarType(const ArgType& type) {
  auto arg_type_iter = kArgTypeToProtoScalarType.find(type);
  if (arg_type_iter == kArgTypeToProtoScalarType.end()) {
    return error::Internal("Could not convert type: $0", magic_enum::enum_name(type));
  }
  return arg_type_iter->second;
}

StatusOr<const ArgInfo*> GetArgInfo(const std::map<std::string, ArgInfo>& args_map,
                                    const std::string& arg_name) {
  auto args_map_iter = args_map.find(arg_name);
  if (args_map_iter == args_map.end()) {
    return error::Internal("Could not find argument $0", arg_name);
  }
  return &args_map_iter->second;
}

StatusOr<dynamictracingpb::PhysicalProbe> AddDwarves(const dynamictracingpb::Probe& input_probe) {
  using dwarf_tools::DwarfReader;

  const std::string& binary_path = input_probe.trace_point().binary_path();
  const std::string& function_symbol = input_probe.trace_point().function_symbol();

  PL_ASSIGN_OR_RETURN(std::unique_ptr<DwarfReader> dwarf_reader, DwarfReader::Create(binary_path));

  PL_ASSIGN_OR_RETURN(auto args_map, dwarf_reader->GetFunctionArgInfo(function_symbol));

  dynamictracingpb::PhysicalProbe out;

  out.mutable_trace_point()->CopyFrom(input_probe.trace_point());
  out.set_type(input_probe.type());

  // Add SP variable.
  {
    auto* var = out.add_vars();
    var->set_name("sp");
    var->set_val_type(dynamictracingpb::VOID_POINTER);
    var->set_reg(dynamictracingpb::Register::SP);
  }

  // Dwarf and BCC have an 8 byte difference in where they believe the SP is.
  // This adjustment factor accounts for that difference.
  constexpr int32_t kSPOffset = 8;

  for (auto& arg : input_probe.args()) {
    // For now, just assume the expression is a simple arg name.
    // TODO(oazizi): Support expressions.
    std::string arg_name = arg.expr();

    PL_ASSIGN_OR_RETURN(const ArgInfo* arg_info, GetArgInfo(args_map, arg_name));
    DCHECK(arg_info != nullptr);

    PL_ASSIGN_OR_RETURN(dynamictracingpb::ScalarType type,
                        ArgTypeToProtoScalarType(arg_info->type));

    auto* var = out.add_vars();
    var->set_name(arg.id());
    var->set_val_type(type);
    var->mutable_memory()->set_base("sp");
    var->mutable_memory()->set_offset(arg_info->offset + kSPOffset);
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

    PL_ASSIGN_OR_RETURN(dynamictracingpb::ScalarType type,
                        ArgTypeToProtoScalarType(arg_info->type));

    auto* var = out.add_vars();
    var->set_name(ret_val.id());
    var->set_val_type(type);
    var->mutable_memory()->set_base("sp");
    var->mutable_memory()->set_offset(arg_info->offset + 8);
  }

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
