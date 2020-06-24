#include "src/stirling/dynamic_tracing/dwarf_info.h"

#include <memory>
#include <string>

#include "src/stirling/obj_tools/dwarf_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

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

  for (auto& arg : input_probe.args()) {
    auto it = args_map.find(arg.expr());
    if (it == args_map.end()) {
      return error::Internal("Could not find argument $0", arg.expr());
    }
    dwarf_tools::ArgInfo& arg_info = it->second;

    auto* var = out.add_vars();
    var->set_name(arg.id());
    var->set_val_type(dynamictracingpb::INT32);
    var->mutable_memory()->set_base("sp");
    var->mutable_memory()->set_offset(arg_info.offset + 8);
  }

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
