#include "src/stirling/dynamic_tracing/dwarf_info.h"

#include <map>
#include <memory>
#include <string>

#include "src/stirling/obj_tools/dwarf_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

// TODO(oazizi): Consider using dynamictracingpb::ScalarType directly in ArgInfo,
//               to avoid this additional mapping.
const std::map<dwarf_tools::ArgType, dynamictracingpb::ScalarType> kArgTypeToProtoScalarType = {
    {dwarf_tools::ArgType::kBool, dynamictracingpb::ScalarType::BOOL},
    {dwarf_tools::ArgType::kInt, dynamictracingpb::ScalarType::INT},
    {dwarf_tools::ArgType::kInt8, dynamictracingpb::ScalarType::INT8},
    {dwarf_tools::ArgType::kInt16, dynamictracingpb::ScalarType::INT16},
    {dwarf_tools::ArgType::kInt32, dynamictracingpb::ScalarType::INT32},
    {dwarf_tools::ArgType::kInt64, dynamictracingpb::ScalarType::INT64},
    {dwarf_tools::ArgType::kUInt, dynamictracingpb::ScalarType::UINT},
    {dwarf_tools::ArgType::kUInt8, dynamictracingpb::ScalarType::UINT8},
    {dwarf_tools::ArgType::kUInt16, dynamictracingpb::ScalarType::UINT16},
    {dwarf_tools::ArgType::kUInt32, dynamictracingpb::ScalarType::UINT32},
    {dwarf_tools::ArgType::kUInt64, dynamictracingpb::ScalarType::UINT64},
    {dwarf_tools::ArgType::kFloat32, dynamictracingpb::ScalarType::FLOAT},
    {dwarf_tools::ArgType::kFloat64, dynamictracingpb::ScalarType::DOUBLE},
    {dwarf_tools::ArgType::kPointer, dynamictracingpb::ScalarType::VOID_POINTER},
};

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
    auto args_map_iter = args_map.find(arg.expr());
    if (args_map_iter == args_map.end()) {
      return error::Internal("Could not find argument $0", arg.expr());
    }
    dwarf_tools::ArgInfo& arg_info = args_map_iter->second;

    auto arg_type_iter = kArgTypeToProtoScalarType.find(arg_info.type);
    if (arg_type_iter == kArgTypeToProtoScalarType.end()) {
      return error::Internal("Could not convert type: $0", magic_enum::enum_name(arg_info.type));
    }
    dynamictracingpb::ScalarType type = arg_type_iter->second;

    auto* var = out.add_vars();
    var->set_name(arg.id());
    var->set_val_type(type);
    var->mutable_memory()->set_base("sp");
    var->mutable_memory()->set_offset(arg_info.offset + kSPOffset);
  }

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
