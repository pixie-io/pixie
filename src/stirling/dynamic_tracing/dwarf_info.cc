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

/**
 * The Dwarvifier generates a PhysicalProbe from a given LogicalProbe spec.
 * The Dwarvifier's job is to:
 *  - To generate variables (with correct offsets) to access argument/return value expressions.
 *  - To add type information to maps and outputs.
 *  - To create the necessary structs to access those maps and outputs.
 * Any referenced maps and outputs must exist in the Logical spec.
 */
class Dwarvifier {
 public:
  Dwarvifier(const std::map<std::string, ir::shared::Map*>& maps,
             const std::map<std::string, ir::shared::Output*>& outputs)
      : maps_(maps), outputs_(outputs) {}
  Status GenerateProbe(const ir::logical::Probe input_probe, ir::physical::Program* output_program);

 private:
  Status Setup(const ir::shared::TracePoint& trace_point);
  Status ProcessProbe(const ir::logical::Probe& input_probe, ir::physical::Program* output_program);
  Status ProcessTracepoint(const ir::shared::TracePoint& trace_point,
                           ir::physical::PhysicalProbe* output_probe);
  Status ProcessSpecialVariables(ir::physical::PhysicalProbe* output_probe);
  Status ProcessArgExpr(const ir::logical::Argument& arg,
                        ir::physical::PhysicalProbe* output_probe);
  Status ProcessRetValExpr(const ir::logical::ReturnValue& ret_val,
                           ir::physical::PhysicalProbe* output_probe);
  Status ProcessStashAction(const ir::logical::MapStashAction& stash_action,
                            ir::physical::PhysicalProbe* output_probe,
                            ir::physical::Program* output_program);
  Status ProcessOutputAction(const ir::logical::OutputAction& output_action,
                             ir::physical::PhysicalProbe* output_probe,
                             ir::physical::Program* output_program);

  Status GenerateMapValueStruct(const ir::logical::MapStashAction& stash_action_in,
                                const std::string& struct_type_name,
                                ir::physical::Program* output_program);
  Status GenerateOutputStruct(const ir::logical::OutputAction& output_action_in,
                              const std::string& struct_type_name,
                              ir::physical::Program* output_program);

  const std::map<std::string, ir::shared::Map*>& maps_;
  const std::map<std::string, ir::shared::Output*>& outputs_;

  std::unique_ptr<dwarf_tools::DwarfReader> dwarf_reader_;
  std::map<std::string, dwarf_tools::ArgInfo> args_map_;
  std::map<std::string, ir::physical::ScalarVariable*> vars_map_;

  // Dwarf and BCC have an 8 byte difference in where they believe the SP is.
  // This adjustment factor accounts for that difference.
  static constexpr int32_t kSPOffset = 8;

  static inline const std::string kSPVarName = "sp";
  static inline const std::string kTGIDVarName = "tgid";
  static inline const std::string kTGIDStartTimeVarName = "tgid_start_time";
  static inline const std::string kGOIDVarName = "goid";
  static inline const std::string kKTimeVarName = "ktime_ns";

  static inline const std::vector<std::string> kImplicitColumns{kTGIDVarName, kTGIDStartTimeVarName,
                                                                kGOIDVarName, kKTimeVarName};

  // We use these values as we build temporary variables for expressions.

  // String for . operator (eg. my_struct.field)
  static constexpr std::string_view kDotStr = "_D_";

  // String for * operator (eg. (*my_struct).field)
  static constexpr std::string_view kDerefStr = "_X_";
};

// AddDwarves is the main entry point.
StatusOr<ir::physical::Program> AddDwarves(const ir::logical::Program& input_program) {
  ir::physical::Program output_program;

  std::map<std::string, ir::shared::Map*> maps;
  std::map<std::string, ir::shared::Output*> outputs;

  // Copy all maps.
  for (const auto& map : input_program.maps()) {
    auto* m = output_program.add_maps();
    m->CopyFrom(map);
    maps[m->name()] = m;
  }

  // Copy all outputs.
  for (const auto& output : input_program.outputs()) {
    auto* o = output_program.add_outputs();
    o->CopyFrom(output);
    outputs[o->name()] = o;
  }

  // Transform probes.
  Dwarvifier dwarvifier(maps, outputs);
  for (const auto& probe : input_program.probes()) {
    PL_RETURN_IF_ERROR(dwarvifier.GenerateProbe(probe, &output_program));
  }

  return output_program;
}

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

Status Dwarvifier::GenerateProbe(const ir::logical::Probe input_probe,
                                 ir::physical::Program* output_program) {
  PL_RETURN_IF_ERROR(Setup(input_probe.trace_point()));
  PL_RETURN_IF_ERROR(ProcessProbe(input_probe, output_program));
  return Status::OK();
}

Status Dwarvifier::Setup(const ir::shared::TracePoint& trace_point) {
  using dwarf_tools::DwarfReader;

  const std::string& binary_path = trace_point.binary_path();
  const std::string& function_symbol = trace_point.symbol();

  PL_ASSIGN_OR_RETURN(dwarf_reader_, DwarfReader::Create(binary_path));
  PL_ASSIGN_OR_RETURN(args_map_, dwarf_reader_->GetFunctionArgInfo(function_symbol));

  return Status::OK();
}

Status Dwarvifier::ProcessTracepoint(const ir::shared::TracePoint& trace_point,
                                     ir::physical::PhysicalProbe* output_probe) {
  auto* probe_trace_point = output_probe->mutable_trace_point();
  probe_trace_point->CopyFrom(trace_point);
  probe_trace_point->set_type(trace_point.type());

  return Status::OK();
}

Status Dwarvifier::ProcessProbe(const ir::logical::Probe& input_probe,
                                ir::physical::Program* output_program) {
  auto* p = output_program->add_probes();

  PL_RETURN_IF_ERROR(ProcessTracepoint(input_probe.trace_point(), p));
  PL_RETURN_IF_ERROR(ProcessSpecialVariables(p));

  for (const auto& arg : input_probe.args()) {
    PL_RETURN_IF_ERROR(ProcessArgExpr(arg, p));
  }

  for (const auto& ret_val : input_probe.ret_vals()) {
    PL_RETURN_IF_ERROR(ProcessRetValExpr(ret_val, p));
  }

  for (const auto& stash_action : input_probe.stash_map_actions()) {
    PL_RETURN_IF_ERROR(ProcessStashAction(stash_action, p, output_program));
  }

  for (const auto& output_action : input_probe.output_actions()) {
    PL_RETURN_IF_ERROR(ProcessOutputAction(output_action, p, output_program));
  }

  return Status::OK();
}

// TODO(oazizi): Could selectively generate some of these variables, when they are not required.
//               For example, if latency is not required, then there is no need for ktime.
//               For now, include them all for simplicity.
Status Dwarvifier::ProcessSpecialVariables(ir::physical::PhysicalProbe* output_probe) {
  // Add SP variable.
  {
    auto* var = output_probe->add_vars();
    var->set_name(kSPVarName);
    var->set_type(ir::shared::VOID_POINTER);
    var->set_reg(ir::physical::Register::SP);

    vars_map_[kSPVarName] = var;
  }

  // Add tgid variable
  {
    auto* var = output_probe->add_vars();
    var->set_name(kTGIDVarName);
    var->set_type(ir::shared::ScalarType::INT32);
    var->set_builtin(ir::shared::BPFHelper::TGID);

    vars_map_[kTGIDVarName] = var;
  }

  // Add TGID start time (required for UPID construction)
  {
    auto* var = output_probe->add_vars();
    var->set_name(kTGIDStartTimeVarName);
    var->set_type(ir::shared::ScalarType::UINT64);
    var->set_builtin(ir::shared::BPFHelper::TGID_START_TIME);

    vars_map_[kTGIDStartTimeVarName] = var;
  }

  // Add goid variable
  {
    auto* var = output_probe->add_vars();
    var->set_name(kGOIDVarName);
    var->set_type(ir::shared::ScalarType::INT32);
    var->set_builtin(ir::shared::BPFHelper::GOID);

    vars_map_[kGOIDVarName] = var;
  }

  // Add current time variable (for latency)
  {
    auto* var = output_probe->add_vars();
    var->set_name(kKTimeVarName);
    var->set_type(ir::shared::ScalarType::UINT64);
    var->set_builtin(ir::shared::BPFHelper::KTIME);

    vars_map_[kKTimeVarName] = var;
  }

  return Status::OK();
}

Status Dwarvifier::ProcessArgExpr(const ir::logical::Argument& arg,
                                  ir::physical::PhysicalProbe* output_probe) {
  const std::string& arg_name = arg.expr();

  std::vector<std::string_view> components = absl::StrSplit(arg_name, ".");

  PL_ASSIGN_OR_RETURN(const ArgInfo* arg_info, GetArgInfo(args_map_, components.front()));
  DCHECK(arg_info != nullptr);

  VarType type = arg_info->type;
  std::string type_name = std::move(arg_info->type_name);
  int offset = kSPOffset + arg_info->offset;
  std::string base = kSPVarName;
  std::string name = arg.id();

  // Note that we start processing at element [1], not [0], which was used to set the starting
  // state in the lines above.
  for (auto iter = components.begin() + 1; iter < components.end(); ++iter) {
    // If parent is a pointer, create a variable to dereference it.
    if (type == VarType::kPointer) {
      PL_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type,
                          VarTypeToProtoScalarType(type, type_name));

      absl::StrAppend(&name, kDerefStr);

      auto* var = output_probe->add_vars();
      var->set_name(name);
      var->set_type(pb_type);
      var->mutable_memory()->set_base(base);
      var->mutable_memory()->set_offset(offset);

      vars_map_[name] = var;

      // Reset base and offset.
      base = name;
      offset = 0;
    }

    std::string_view field_name = *iter;
    PL_ASSIGN_OR_RETURN(VarInfo member_info,
                        dwarf_reader_->GetStructMemberInfo(type_name, field_name));
    offset += member_info.offset;
    type_name = std::move(member_info.type_name);
    type = member_info.type;
    absl::StrAppend(&name, kDotStr, field_name);
  }

  // If parent is a pointer, create a variable to dereference it.
  if (type == VarType::kPointer) {
    PL_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type, VarTypeToProtoScalarType(type, type_name));

    absl::StrAppend(&name, kDerefStr);

    auto* var = output_probe->add_vars();
    var->set_name(name);
    var->set_type(pb_type);
    var->mutable_memory()->set_base(base);
    var->mutable_memory()->set_offset(offset);

    vars_map_[name] = var;

    // Reset base and offset.
    base = name;
    offset = 0;

    // Since we are the leaf, also force the type to a BaseType.
    // If we are not, in fact, at a base type, then VarTypeToProtoScalarType will error out,
    // as it should--since we can't trace non-base types.
    type = VarType::kBaseType;
    absl::StrAppend(&name, kDerefStr);
  }

  PL_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type, VarTypeToProtoScalarType(type, type_name));

  // The very last created variable uses the original id.
  // This is important so that references in the original probe are maintained.
  name = arg.id();

  auto* var = output_probe->add_vars();
  var->set_name(name);
  var->set_type(pb_type);
  var->mutable_memory()->set_base(base);
  var->mutable_memory()->set_offset(offset);

  vars_map_[name] = var;

  return Status::OK();
}

Status Dwarvifier::ProcessRetValExpr(const ir::logical::ReturnValue& ret_val,
                                     ir::physical::PhysicalProbe* output_probe) {
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

  PL_ASSIGN_OR_RETURN(const ArgInfo* arg_info, GetArgInfo(args_map_, ret_val_name));
  DCHECK(arg_info != nullptr);

  PL_ASSIGN_OR_RETURN(ir::shared::ScalarType type,
                      VarTypeToProtoScalarType(arg_info->type, arg_info->type_name));

  auto* var = output_probe->add_vars();
  var->set_name(ret_val.id());
  var->set_type(type);
  var->mutable_memory()->set_base("sp");
  var->mutable_memory()->set_offset(arg_info->offset + kSPOffset);

  return Status::OK();
}

Status Dwarvifier::GenerateMapValueStruct(const ir::logical::MapStashAction& stash_action_in,
                                          const std::string& struct_type_name,
                                          ir::physical::Program* output_program) {
  // TODO(oazizi): Check if struct already exists. If it does, make sure it is the same.

  auto* struct_decl = output_program->add_structs();
  struct_decl->set_name(struct_type_name);

  for (const auto& f : stash_action_in.value_variable_name()) {
    auto* struct_field = struct_decl->add_fields();
    struct_field->set_name(absl::StrCat(stash_action_in.map_name(), "_", f));

    auto iter = vars_map_.find(f);
    if (iter == vars_map_.end()) {
      return error::Internal("StashAction: Reference to unknown variable $0", f);
    }
    struct_field->mutable_type()->set_scalar(iter->second->type());
  }

  return Status::OK();
}

namespace {
Status PopulateMapTypes(const std::map<std::string, ir::shared::Map*>& maps,
                        const std::string& map_name, const std::string& struct_type_name) {
  auto iter = maps.find(map_name);
  if (iter == maps.end()) {
    return error::Internal("Reference to undeclared map: $0", map_name);
  }

  auto* map = iter->second;

  // TODO(oazizi): Check if values are already set. If they are check for consistency.
  map->mutable_key_type()->set_scalar(ir::shared::ScalarType::UINT64);
  map->mutable_value_type()->set_struct_type(struct_type_name);

  return Status::OK();
}
}  // namespace

Status Dwarvifier::ProcessStashAction(const ir::logical::MapStashAction& stash_action_in,
                                      ir::physical::PhysicalProbe* output_probe,
                                      ir::physical::Program* output_program) {
  std::string variable_name = stash_action_in.map_name() + "_value";
  std::string struct_type_name = stash_action_in.map_name() + "_value_t";

  PL_RETURN_IF_ERROR(GenerateMapValueStruct(stash_action_in, struct_type_name, output_program));
  PL_RETURN_IF_ERROR(PopulateMapTypes(maps_, stash_action_in.map_name(), struct_type_name));

  auto* struct_var = output_probe->add_st_vars();
  struct_var->set_type(struct_type_name);
  struct_var->set_name(variable_name);
  for (const auto& f : stash_action_in.value_variable_name()) {
    auto* field = struct_var->add_variable_names();
    field->set_name(f);
  }

  auto* stash_action_out = output_probe->add_map_stash_actions();
  stash_action_out->set_map_name(stash_action_in.map_name());
  // TODO(oazizi): Temporarily hard-coded. Fix.
  stash_action_out->set_key_variable_name("goid");
  stash_action_out->set_value_variable_name(variable_name);

  return Status::OK();
}

Status Dwarvifier::GenerateOutputStruct(const ir::logical::OutputAction& output_action_in,
                                        const std::string& struct_type_name,
                                        ir::physical::Program* output_program) {
  // TODO(oazizi): Check if struct already exists. If it does, make sure it is the same.

  auto* struct_decl = output_program->add_structs();
  struct_decl->set_name(struct_type_name);

  for (const auto& f : kImplicitColumns) {
    auto* struct_field = struct_decl->add_fields();
    struct_field->set_name(absl::StrCat(output_action_in.output_name(), "_", f));

    auto iter = vars_map_.find(f);
    if (iter == vars_map_.end()) {
      return error::Internal("OutputAction: Reference to unknown variable $0", f);
    }

    struct_field->mutable_type()->set_scalar(iter->second->type());
  }

  for (const auto& f : output_action_in.variable_name()) {
    auto* struct_field = struct_decl->add_fields();
    struct_field->set_name(absl::StrCat(output_action_in.output_name(), "_", f));

    auto iter = vars_map_.find(f);
    if (iter == vars_map_.end()) {
      return error::Internal("OutputAction: Reference to unknown variable $0", f);
    }
    struct_field->mutable_type()->set_scalar(iter->second->type());
  }

  return Status::OK();
}

namespace {
Status PopulateOutputTypes(const std::map<std::string, ir::shared::Output*>& outputs,
                           const std::string& output_name, const std::string& struct_type_name) {
  auto iter = outputs.find(output_name);
  if (iter == outputs.end()) {
    return error::Internal("Reference to undeclared map: $0", output_name);
  }

  auto* output = iter->second;

  // TODO(oazizi): Check if values are already set. If they are check for consistency.
  output->mutable_type()->set_struct_type(struct_type_name);

  return Status::OK();
}
}  // namespace

Status Dwarvifier::ProcessOutputAction(const ir::logical::OutputAction& output_action_in,
                                       ir::physical::PhysicalProbe* output_probe,
                                       ir::physical::Program* output_program) {
  std::string variable_name = output_action_in.output_name() + "_value";
  std::string struct_type_name = output_action_in.output_name() + "_value_t";

  // Generate struct definition.
  PL_RETURN_IF_ERROR(GenerateOutputStruct(output_action_in, struct_type_name, output_program));

  // Generate an output definition.
  PL_RETURN_IF_ERROR(
      PopulateOutputTypes(outputs_, output_action_in.output_name(), struct_type_name));

  // Create and initialize a struct variable.
  auto* struct_var = output_probe->add_st_vars();
  struct_var->set_type(struct_type_name);
  struct_var->set_name(variable_name);
  for (const auto& f : kImplicitColumns) {
    auto* field = struct_var->add_variable_names();
    field->set_name(f);
  }
  for (const auto& f : output_action_in.variable_name()) {
    auto* field = struct_var->add_variable_names();
    field->set_name(f);
  }

  // Output data.
  auto* output_action_out = output_probe->add_output_actions();
  output_action_out->set_perf_buffer_name(output_action_in.output_name());
  output_action_out->set_variable_name(variable_name);

  return Status::OK();
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
