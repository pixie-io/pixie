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
 * The Dwarvifier generates a PhysicalProbe from an Intermediate Probe spec.
 * It follows a builder model. The initial Dwarvifier is empty;
 * Successive calls to Process functions builds the internal model of the new PhysicalProbe.
 * When complete, a call to ConsumeResult() emits the generated protobuf.
 */
class Dwarvifier {
 public:
  Dwarvifier() {}

  // This must be the first call, as the dwarf info is extracted from the binary in the tracepoint.
  Status ProcessTracepoint(const ir::shared::TracePoint& trace_point);
  void ProcessSpecialVariables();
  Status ProcessArgExpr(const ir::logical::Argument& arg);
  Status ProcessRetValExpr(const ir::logical::ReturnValue& ret_val);
  Status ProcessStashAction(const ir::shared::LogicalMapStashAction& stash_action);
  Status ProcessOutputAction(const ir::shared::LogicalOutputAction& output_action);
  ir::physical::Program ConsumeResult();

 private:
  std::unique_ptr<dwarf_tools::DwarfReader> dwarf_reader_;
  std::map<std::string, dwarf_tools::ArgInfo> args_map_;
  std::map<std::string, ir::physical::ScalarVariable*> vars_map_;
  ir::physical::PhysicalProbe probe_;
  std::map<std::string, ir::physical::Struct> structs_;
  std::map<std::string, ir::shared::Map> maps_;
  std::map<std::string, ir::shared::Output> outputs_;

  // Dwarf and BCC have an 8 byte difference in where they believe the SP is.
  // This adjustment factor accounts for that difference.
  static constexpr int32_t kSPOffset = 8;

  static inline const std::string kSPVarName = "sp";
  static inline const std::string kTGIDVarName = "tgid";
  static inline const std::string kTGIDStartTimeVarName = "tgid_start_time";
  static inline const std::string kGOIDVarName = "goid";
  static inline const std::string kKTimeVarName = "ktime_ns";

  // We use these values as we build temporary variables for expressions.

  // String for . operator (eg. my_struct.field)
  static constexpr std::string_view kDotStr = "_D_";

  // String for * operator (eg. (*my_struct).field)
  static constexpr std::string_view kDerefStr = "_X_";
};

StatusOr<ir::physical::Program> AddDwarves(const ir::logical::Probe& input_probe) {
  Dwarvifier dwarvifier;
  PL_RETURN_IF_ERROR(dwarvifier.ProcessTracepoint(input_probe.trace_point()));

  dwarvifier.ProcessSpecialVariables();

  for (auto& arg : input_probe.args()) {
    PL_RETURN_IF_ERROR(dwarvifier.ProcessArgExpr(arg));
  }

  for (auto& ret_val : input_probe.ret_vals()) {
    PL_RETURN_IF_ERROR(dwarvifier.ProcessRetValExpr(ret_val));
  }

  for (auto& stash_action : input_probe.stash_map_actions()) {
    PL_RETURN_IF_ERROR(dwarvifier.ProcessStashAction(stash_action));
  }

  for (auto& output_action : input_probe.output_actions()) {
    PL_RETURN_IF_ERROR(dwarvifier.ProcessOutputAction(output_action));
  }

  return dwarvifier.ConsumeResult();
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

Status Dwarvifier::ProcessTracepoint(const ir::shared::TracePoint& trace_point) {
  using dwarf_tools::DwarfReader;

  const std::string& binary_path = trace_point.binary_path();
  const std::string& function_symbol = trace_point.symbol();

  PL_ASSIGN_OR_RETURN(dwarf_reader_, DwarfReader::Create(binary_path));
  PL_ASSIGN_OR_RETURN(args_map_, dwarf_reader_->GetFunctionArgInfo(function_symbol));

  auto* probe_trace_point = probe_.mutable_trace_point();
  probe_trace_point->CopyFrom(trace_point);
  probe_trace_point->set_type(trace_point.type());

  return Status::OK();
}

// TODO(oazizi): Could selectively generate some of these variables, when they are not required.
//               For example, if latency is not required, then there is no need for ktime.
//               For now, include them all for simplicity.
void Dwarvifier::ProcessSpecialVariables() {
  // Add SP variable.
  {
    auto* var = probe_.add_vars();
    var->set_name(kSPVarName);
    var->set_type(ir::shared::VOID_POINTER);
    var->set_reg(ir::physical::Register::SP);

    vars_map_[kSPVarName] = var;
  }

  // Add tgid variable
  {
    auto* var = probe_.add_vars();
    var->set_name(kTGIDVarName);
    var->set_type(ir::shared::ScalarType::INT32);
    var->set_builtin(ir::shared::BPFHelper::TGID);

    vars_map_[kTGIDVarName] = var;
  }

  // Add TGID start time (required for UPID construction)
  {
    auto* var = probe_.add_vars();
    var->set_name(kTGIDStartTimeVarName);
    var->set_type(ir::shared::ScalarType::UINT64);
    var->set_builtin(ir::shared::BPFHelper::TGID_START_TIME);

    vars_map_[kTGIDStartTimeVarName] = var;
  }

  // Add goid variable
  {
    auto* var = probe_.add_vars();
    var->set_name(kGOIDVarName);
    var->set_type(ir::shared::ScalarType::INT32);
    var->set_builtin(ir::shared::BPFHelper::GOID);

    vars_map_[kGOIDVarName] = var;
  }

  // Add current time variable (for latency)
  {
    auto* var = probe_.add_vars();
    var->set_name(kKTimeVarName);
    var->set_type(ir::shared::ScalarType::UINT64);
    var->set_builtin(ir::shared::BPFHelper::KTIME);

    vars_map_[kKTimeVarName] = var;
  }
}

Status Dwarvifier::ProcessArgExpr(const ir::logical::Argument& arg) {
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

      auto* var = probe_.add_vars();
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

    auto* var = probe_.add_vars();
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

  auto* var = probe_.add_vars();
  var->set_name(name);
  var->set_type(pb_type);
  var->mutable_memory()->set_base(base);
  var->mutable_memory()->set_offset(offset);

  vars_map_[name] = var;

  return Status::OK();
}

Status Dwarvifier::ProcessRetValExpr(const ir::logical::ReturnValue& ret_val) {
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

  auto* var = probe_.add_vars();
  var->set_name(ret_val.id());
  var->set_type(type);
  var->mutable_memory()->set_base("sp");
  var->mutable_memory()->set_offset(arg_info->offset + kSPOffset);

  return Status::OK();
}

Status Dwarvifier::ProcessStashAction(const ir::shared::LogicalMapStashAction& stash_action_in) {
  std::string variable_name = stash_action_in.map_name() + "_value";
  std::string struct_type_name = stash_action_in.map_name() + "_value_t";

  auto& struct_decl = structs_[struct_type_name];
  struct_decl.set_name(struct_type_name);
  for (auto& f : stash_action_in.value_variable_name()) {
    auto* struct_field = struct_decl.add_fields();
    struct_field->set_name(absl::StrCat(stash_action_in.map_name(), "_", f));

    auto iter = vars_map_.find(f);
    if (iter == vars_map_.end()) {
      return error::Internal("StashAction: Reference to unknown variable $0", f);
    }
    struct_field->mutable_type()->set_scalar(iter->second->type());
  }

  auto& map = maps_[stash_action_in.map_name()];
  map.set_name(stash_action_in.map_name());
  map.mutable_key_type()->set_scalar(ir::shared::ScalarType::UINT64);
  map.mutable_value_type()->set_struct_type(struct_type_name);

  auto* struct_var = probe_.add_st_vars();
  struct_var->set_type(struct_type_name);
  struct_var->set_name(variable_name);
  for (auto& f : stash_action_in.value_variable_name()) {
    auto* field = struct_var->add_variable_names();
    field->set_name(f);
  }

  auto* stash_action_out = probe_.add_map_stash_actions();
  stash_action_out->set_map_name(stash_action_in.map_name());
  // TODO(oazizi): Temporarily hard-coded. Fix.
  stash_action_out->set_key_variable_name("goid");
  stash_action_out->set_value_variable_name(variable_name);

  return Status::OK();
}

Status Dwarvifier::ProcessOutputAction(const ir::shared::LogicalOutputAction& output_action_in) {
  std::string variable_name = absl::StrCat(output_action_in.output_name(), "_value");
  std::string struct_type_name = absl::StrCat(output_action_in.output_name(), "_value_t");

  auto& struct_decl = structs_[struct_type_name];
  struct_decl.set_name(struct_type_name);

  const std::vector<std::string> kImplicitColumns{kTGIDVarName, kTGIDStartTimeVarName, kGOIDVarName,
                                                  kKTimeVarName};

  for (auto& f : kImplicitColumns) {
    auto* struct_field = struct_decl.add_fields();
    struct_field->set_name(absl::StrCat(output_action_in.output_name(), "_", f));

    auto iter = vars_map_.find(f);
    if (iter == vars_map_.end()) {
      return error::Internal("OutputAction: Reference to unknown variable $0", f);
    }

    struct_field->mutable_type()->set_scalar(iter->second->type());
  }

  for (auto& f : output_action_in.variable_name()) {
    auto* struct_field = struct_decl.add_fields();
    struct_field->set_name(absl::StrCat(output_action_in.output_name(), "_", f));

    auto iter = vars_map_.find(f);
    if (iter == vars_map_.end()) {
      return error::Internal("OutputAction: Reference to unknown variable $0", f);
    }
    struct_field->mutable_type()->set_scalar(iter->second->type());
  }

  auto* struct_var = probe_.add_st_vars();
  struct_var->set_type(struct_type_name);
  struct_var->set_name(variable_name);
  for (auto& f : kImplicitColumns) {
    auto* field = struct_var->add_variable_names();
    field->set_name(f);
  }
  for (auto& f : output_action_in.variable_name()) {
    auto* field = struct_var->add_variable_names();
    field->set_name(f);
  }

  auto& output = outputs_[output_action_in.output_name()];
  output.set_name(output_action_in.output_name());
  output.mutable_type()->set_struct_type(struct_type_name);

  auto* output_action_out = probe_.add_output_actions();
  output_action_out->set_perf_buffer_name(output_action_in.output_name());
  output_action_out->set_variable_name(variable_name);

  return Status::OK();
}

ir::physical::Program Dwarvifier::ConsumeResult() {
  ir::physical::Program out;
  out.add_probes()->CopyFrom(probe_);

  for (auto& [name, s] : structs_) {
    out.add_structs()->CopyFrom(s);
  }

  for (auto& [name, m] : maps_) {
    out.add_maps()->CopyFrom(m);
  }

  for (auto& [name, o] : outputs_) {
    out.add_outputs()->CopyFrom(o);
  }

  return out;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
