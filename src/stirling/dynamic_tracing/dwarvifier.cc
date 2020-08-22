#include "src/stirling/dynamic_tracing/dwarvifier.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/stirling/dynamic_tracing/ir/sharedpb/shared.pb.h"
#include "src/stirling/obj_tools/dwarf_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dwarf_tools::ArgInfo;
using ::pl::stirling::dwarf_tools::LocationType;
using ::pl::stirling::dwarf_tools::StructMemberInfo;
using ::pl::stirling::dwarf_tools::TypeInfo;
using ::pl::stirling::dwarf_tools::VarType;

using ::pl::stirling::dynamic_tracing::ir::physical::MapVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::MemberVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::StructVariable;

//-----------------------------------------------------------------------------
// Top-level code
//-----------------------------------------------------------------------------

/**
 * The Dwarvifier generates a Probe from a given LogicalProbe spec.
 * The Dwarvifier's job is to:
 *  - To generate variables (with correct offsets) to access argument/return value expressions.
 *  - To add type information to maps and outputs.
 *  - To create the necessary structs to access those maps and outputs.
 * Any referenced maps and outputs must exist in the Logical spec.
 */
class Dwarvifier {
 public:
  Status Setup(const ir::shared::DeploymentSpec& deployment_spec, ir::shared::Language language);
  Status Generate(const ir::logical::TracepointSpec& input_program,
                  ir::physical::Program* output_program);

 private:
  void GenerateMap(const ir::shared::Map& map, ir::physical::Program* output_program);
  void GenerateOutput(const ir::logical::Output& output, ir::physical::Program* output_program);
  Status GenerateProbe(const ir::logical::Probe& input_probe,
                       ir::physical::Program* output_program);
  Status ProcessTracepoint(const ir::shared::Tracepoint& tracepoint,
                           ir::physical::Probe* output_probe);
  void AddSpecialVariables(ir::physical::Probe* output_probe);
  void AddStandardVariables(ir::physical::Probe* output_probe);
  void AddEntryProbeVariables(ir::physical::Probe* output_probe);
  void AddRetProbeVariables(ir::physical::Probe* output_probe);
  Status ProcessConstants(const ir::logical::Constant& constant, ir::physical::Probe* output_probe);

  // The input components describes a sequence of field of nesting structures. The first component
  // is the name of an input argument of a function, or an expression to describe the index of an
  // return value of the function.
  Status ProcessVarExpr(const std::string& var_name, const ArgInfo& arg_info,
                        const std::string& base_var,
                        const std::vector<std::string_view>& components,
                        ir::physical::Probe* output_probe);

  Status ProcessArgExpr(const ir::logical::Argument& arg, ir::physical::Probe* output_probe);
  Status ProcessRetValExpr(const ir::logical::ReturnValue& ret_val,
                           ir::physical::Probe* output_probe);
  Status ProcessMapVal(const ir::logical::MapValue& map_val, ir::physical::Probe* output_probe);
  Status ProcessFunctionLatency(const ir::shared::FunctionLatency& latency,
                                ir::physical::Probe* output_probe);
  Status ProcessStashAction(const ir::logical::MapStashAction& stash_action,
                            ir::physical::Probe* output_probe,
                            ir::physical::Program* output_program);
  Status ProcessDeleteAction(const ir::logical::MapDeleteAction& stash_action,
                             ir::physical::Probe* output_probe);
  Status ProcessOutputAction(const ir::logical::OutputAction& output_action,
                             ir::physical::Probe* output_probe,
                             ir::physical::Program* output_program);

  // TVarType can be ScalarVariable, MemberVariable, etc.
  template <typename TVarType>
  TVarType* AddVariable(ir::physical::Probe* probe, const std::string& name,
                        ir::shared::ScalarType type);

  Status GenerateMapValueStruct(const ir::logical::MapStashAction& stash_action_in,
                                const std::string& struct_type_name,
                                ir::physical::Program* output_program);
  Status GenerateOutputStruct(const ir::logical::OutputAction& output_action_in,
                              const std::string& struct_type_name,
                              ir::physical::Program* output_program);

  std::map<std::string, ir::shared::Map*> maps_;
  std::map<std::string, ir::physical::PerfBufferOutput*> outputs_;
  std::map<std::string, ir::physical::Struct*> structs_;

  std::unique_ptr<dwarf_tools::DwarfReader> dwarf_reader_;

  std::map<std::string, dwarf_tools::ArgInfo> args_map_;
  dwarf_tools::RetValInfo retval_info_;

  // All defined variables.
  absl::flat_hash_map<std::string, ir::shared::ScalarType> variables_;

  ir::shared::Language language_;
  std::vector<std::string> implicit_columns_;
};

// GeneratePhysicalProgram is the main entry point.
StatusOr<ir::physical::Program> GeneratePhysicalProgram(
    const ir::logical::TracepointDeployment& input) {
  if (input.tracepoints_size() != 1) {
    return error::InvalidArgument("Right now only support exactly 1 Tracepoint, got '$0'",
                                  input.tracepoints_size());
  }

  ir::physical::Program output_program;

  output_program.mutable_deployment_spec()->CopyFrom(input.deployment_spec());
  output_program.set_language(input.tracepoints(0).program().language());

  for (const auto& input_tracepoint : input.tracepoints()) {
    // Transform tracepoint program.
    Dwarvifier dwarvifier;
    PL_RETURN_IF_ERROR(
        dwarvifier.Setup(input.deployment_spec(), input_tracepoint.program().language()));
    PL_RETURN_IF_ERROR(dwarvifier.Generate(input_tracepoint.program(), &output_program));
  }

  return output_program;
}

//-----------------------------------------------------------------------------
// Static helper functions
//-----------------------------------------------------------------------------

namespace {

// Special variables all end with an underscore to minimize chance of conflict with user variables.
// User variables that end with underscore are not allowed (this is not yet enforced).
constexpr char kSPVarName[] = "sp_";
constexpr char kTGIDVarName[] = "tgid_";
constexpr char kTGIDPIDVarName[] = "tgid_pid_";
constexpr char kTGIDStartTimeVarName[] = "tgid_start_time_";
constexpr char kGOIDVarName[] = "goid_";
// WARNING: Do not change the name of kKTimeVarName.
//          "time_" is a name implicitly used by the query engine as the time column.
constexpr char kKTimeVarName[] = "time_";
constexpr char kStartKTimeNSVarName[] = "start_ktime_ns";
constexpr char kRCVarName[] = "rc_";
constexpr char kRCPtrVarName[] = "rc__";
constexpr char kParmPtrVarName[] = "parm__";

StatusOr<std::string> BPFHelperVariableName(ir::shared::BPFHelper builtin) {
  static const absl::flat_hash_map<ir::shared::BPFHelper, std::string_view> kBuiltinVarNames = {
      {ir::shared::BPFHelper::GOID, kGOIDVarName},
      {ir::shared::BPFHelper::TGID, kTGIDVarName},
      {ir::shared::BPFHelper::TGID_PID, kTGIDPIDVarName},
      {ir::shared::BPFHelper::TGID_START_TIME, kTGIDStartTimeVarName},
      {ir::shared::BPFHelper::KTIME, kKTimeVarName},
  };
  auto iter = kBuiltinVarNames.find(builtin);
  if (iter == kBuiltinVarNames.end()) {
    return error::NotFound("BPFHelper '$0' does not have a predefined variable",
                           magic_enum::enum_name(builtin));
  }
  return std::string(iter->second);
}

// Returns the struct name associated with an Output or Map declaration.
std::string StructTypeName(const std::string& obj_name) {
  return absl::StrCat(obj_name, "_value_t");
}

// Map to convert Go Base types to ScalarType.
// clang-format off
const absl::flat_hash_map<std::string_view, ir::shared::ScalarType> kGoTypesMap = {
        {"bool",    ir::shared::ScalarType::BOOL},
        {"int",     ir::shared::ScalarType::INT},
        {"int8",    ir::shared::ScalarType::INT8},
        {"int16",   ir::shared::ScalarType::INT16},
        {"int32",   ir::shared::ScalarType::INT32},
        {"int64",   ir::shared::ScalarType::INT64},
        {"uint",    ir::shared::ScalarType::UINT},
        {"uint8",   ir::shared::ScalarType::UINT8},
        {"uint16",  ir::shared::ScalarType::UINT16},
        {"uint32",  ir::shared::ScalarType::UINT32},
        {"uint64",  ir::shared::ScalarType::UINT64},
        {"float32", ir::shared::ScalarType::FLOAT},
        {"float64", ir::shared::ScalarType::DOUBLE},
};

// TODO(oazizi): Keep building this map out.
// Reference: https://en.cppreference.com/w/cpp/language/types
const absl::flat_hash_map<std::string_view, ir::shared::ScalarType> kCPPTypesMap = {
        {"bool",                   ir::shared::ScalarType::BOOL},

        {"short",                  ir::shared::ScalarType::SHORT},
        {"unsigned short",         ir::shared::ScalarType::USHORT},
        {"int",                    ir::shared::ScalarType::INT},
        {"unsigned int",           ir::shared::ScalarType::UINT},
        {"long int",               ir::shared::ScalarType::LONG},
        {"long unsigned int",      ir::shared::ScalarType::ULONG},
        {"long long int",          ir::shared::ScalarType::LONGLONG},
        {"long long unsigned int", ir::shared::ScalarType::ULONGLONG},

        {"char",                   ir::shared::ScalarType::CHAR},
        {"signed char",            ir::shared::ScalarType::CHAR},
        {"unsigned char",          ir::shared::ScalarType::UCHAR},

        {"double",                 ir::shared::ScalarType::DOUBLE},
        {"float",                  ir::shared::ScalarType::FLOAT},
};

const absl::flat_hash_map<std::string_view, ir::shared::ScalarType> kEmptyTypesMap = {};
// clang-format on

const absl::flat_hash_map<std::string_view, ir::shared::ScalarType>& GetTypesMap(
    ir::shared::Language language) {
  switch (language) {
    case ir::shared::Language::GOLANG:
      return kGoTypesMap;
    case ir::shared::Language::C:
    case ir::shared::Language::CPP:
      return kCPPTypesMap;
    default:
      return kEmptyTypesMap;
  }
}

StatusOr<ir::shared::ScalarType> VarTypeToProtoScalarType(const TypeInfo& type_info,
                                                          ir::shared::Language language) {
  switch (type_info.type) {
    case VarType::kBaseType: {
      auto& types_map = GetTypesMap(language);
      auto iter = types_map.find(type_info.type_name);
      if (iter == types_map.end()) {
        return error::Internal("Unrecognized base type: $0", type_info.type_name);
      }
      return iter->second;
    }
    case VarType::kPointer:
      return ir::shared::ScalarType::VOID_POINTER;
    default:
      return error::Internal("Unhandled type: $0", type_info.ToString());
  }
}

StatusOr<ArgInfo> GetArgInfo(const std::map<std::string, ArgInfo>& args_map,
                             std::string_view arg_name) {
  auto args_map_iter = args_map.find(std::string(arg_name));
  if (args_map_iter == args_map.end()) {
    return error::Internal("Could not find argument $0", arg_name);
  }
  ArgInfo retval = args_map_iter->second;

  if (retval.location.loc_type == LocationType::kStack) {
    // The offset of an argument of the stack starts at 8.
    // This is because the last thing on the stack on a function call is the return address.
    constexpr int32_t kSPOffset = 8;
    retval.location.offset += kSPOffset;
  }

  return retval;
}

}  // namespace

//-----------------------------------------------------------------------------
// Dwarvifier
//-----------------------------------------------------------------------------

Status Dwarvifier::Setup(const ir::shared::DeploymentSpec& deployment_spec,
                         ir::shared::Language language) {
  using dwarf_tools::DwarfReader;

  PL_ASSIGN_OR_RETURN(dwarf_reader_, DwarfReader::Create(deployment_spec.path()));

  language_ = language;

  implicit_columns_ = {kTGIDVarName, kTGIDStartTimeVarName, kKTimeVarName};
  if (language_ == ir::shared::Language::GOLANG) {
    implicit_columns_.push_back(kGOIDVarName);
  }

  return Status::OK();
}

Status Dwarvifier::Generate(const ir::logical::TracepointSpec& input_program,
                            ir::physical::Program* output_program) {
  // Copy all maps.
  for (const auto& map : input_program.maps()) {
    GenerateMap(map, output_program);
  }

  // Copy all outputs.
  for (const auto& output : input_program.outputs()) {
    GenerateOutput(output, output_program);
  }

  // Transform probes.
  for (const auto& probe : input_program.probes()) {
    PL_RETURN_IF_ERROR(GenerateProbe(probe, output_program));
  }

  return Status::OK();
}

template <typename TVarType>
TVarType* Dwarvifier::AddVariable(ir::physical::Probe* probe, const std::string& name,
                                  ir::shared::ScalarType type) {
  TVarType* var;

  if constexpr (std::is_same_v<TVarType, ScalarVariable>) {
    var = probe->add_vars()->mutable_scalar_var();
  } else if constexpr (std::is_same_v<TVarType, MemberVariable>) {
    var = probe->add_vars()->mutable_member_var();
  } else if constexpr (std::is_same_v<TVarType, StructVariable>) {
    var = probe->add_vars()->mutable_struct_var();
  } else if constexpr (std::is_same_v<TVarType, MapVariable>) {
    var = probe->add_vars()->mutable_map_var();
  } else {
    COMPILE_TIME_ASSERT(false, "Unsupported ir::physical::Variable type");
  }

  var->set_name(name);
  var->set_type(type);

  // Populate map, so we can lookup this variable later.
  variables_[name] = type;

  return var;
}

void Dwarvifier::GenerateMap(const ir::shared::Map& map, ir::physical::Program* output_program) {
  auto* m = output_program->add_maps();
  m->CopyFrom(map);

  // Record this map (for quick lookup by GenerateProbe).
  maps_[m->name()] = m;
}

void Dwarvifier::GenerateOutput(const ir::logical::Output& output,
                                ir::physical::Program* output_program) {
  auto* o = output_program->add_outputs();

  o->set_name(output.name());
  o->mutable_fields()->CopyFrom(output.fields());
  // Also insert the name of the struct that holds the output variables.
  o->set_struct_type(StructTypeName(output.name()));

  // Record this output (for quick lookup by GenerateProbe).
  outputs_[o->name()] = o;
}

namespace {

bool IsFunctionLatencySpecified(const ir::logical::Probe& probe) {
  return probe.function_latency_oneof_case() ==
         ir::logical::Probe::FunctionLatencyOneofCase::kFunctionLatency;
}

}  // namespace

Status Dwarvifier::GenerateProbe(const ir::logical::Probe& input_probe,
                                 ir::physical::Program* output_program) {
  // Some initial setup.
  variables_.clear();
  PL_ASSIGN_OR_RETURN(args_map_,
                      dwarf_reader_->GetFunctionArgInfo(input_probe.tracepoint().symbol()));
  PL_ASSIGN_OR_RETURN(retval_info_,
                      dwarf_reader_->GetFunctionRetValInfo(input_probe.tracepoint().symbol()));

  auto* p = output_program->add_probes();

  p->set_name(input_probe.name());

  PL_RETURN_IF_ERROR(ProcessTracepoint(input_probe.tracepoint(), p));
  AddSpecialVariables(p);

  for (auto& constant : input_probe.consts()) {
    PL_RETURN_IF_ERROR(ProcessConstants(constant, p));
  }

  for (const auto& arg : input_probe.args()) {
    PL_RETURN_IF_ERROR(ProcessArgExpr(arg, p));
  }

  for (const auto& ret_val : input_probe.ret_vals()) {
    PL_RETURN_IF_ERROR(ProcessRetValExpr(ret_val, p));
  }

  for (const auto& map_val : input_probe.map_vals()) {
    PL_RETURN_IF_ERROR(ProcessMapVal(map_val, p));
  }

  if (IsFunctionLatencySpecified(input_probe)) {
    PL_RETURN_IF_ERROR(ProcessFunctionLatency(input_probe.function_latency(), p));
  }

  for (const auto& stash_action : input_probe.map_stash_actions()) {
    PL_RETURN_IF_ERROR(ProcessStashAction(stash_action, p, output_program));
  }

  for (const auto& delete_action : input_probe.map_delete_actions()) {
    PL_RETURN_IF_ERROR(ProcessDeleteAction(delete_action, p));
  }

  for (const auto& output_action : input_probe.output_actions()) {
    PL_RETURN_IF_ERROR(ProcessOutputAction(output_action, p, output_program));
  }

  for (const auto& printk : input_probe.printks()) {
    p->add_printks()->CopyFrom(printk);
  }

  return Status::OK();
}

Status Dwarvifier::ProcessTracepoint(const ir::shared::Tracepoint& tracepoint,
                                     ir::physical::Probe* output_probe) {
  auto* probe_tracepoint = output_probe->mutable_tracepoint();
  probe_tracepoint->CopyFrom(tracepoint);
  probe_tracepoint->set_type(tracepoint.type());

  return Status::OK();
}

void Dwarvifier::AddSpecialVariables(ir::physical::Probe* output_probe) {
  AddStandardVariables(output_probe);

  if (output_probe->tracepoint().type() == ir::shared::Tracepoint::ENTRY) {
    AddEntryProbeVariables(output_probe);
  }

  if (output_probe->tracepoint().type() == ir::shared::Tracepoint::RETURN) {
    AddRetProbeVariables(output_probe);
  }
}

// TODO(oazizi): Could selectively generate some of these variables, when they are not required.
//               For example, if latency is not required, then there is no need for ktime.
//               For now, include them all for simplicity.
void Dwarvifier::AddStandardVariables(ir::physical::Probe* output_probe) {
  // Add SP variable.
  auto* sp_var = AddVariable<ScalarVariable>(output_probe, kSPVarName, ir::shared::VOID_POINTER);
  sp_var->set_reg(ir::physical::Register::SP);

  // Add tgid variable.
  auto* tgid_var =
      AddVariable<ScalarVariable>(output_probe, kTGIDVarName, ir::shared::ScalarType::INT32);
  tgid_var->set_builtin(ir::shared::BPFHelper::TGID);

  // Add tgid_pid variable.
  auto* tgid_pid_var =
      AddVariable<ScalarVariable>(output_probe, kTGIDPIDVarName, ir::shared::ScalarType::UINT64);
  tgid_pid_var->set_builtin(ir::shared::BPFHelper::TGID_PID);

  // Add TGID start time (required for UPID construction).
  auto* tgid_start_time_var = AddVariable<ScalarVariable>(output_probe, kTGIDStartTimeVarName,
                                                          ir::shared::ScalarType::UINT64);
  tgid_start_time_var->set_builtin(ir::shared::BPFHelper::TGID_START_TIME);

  // Add current time variable (for latency).
  auto* ktime_var =
      AddVariable<ScalarVariable>(output_probe, kKTimeVarName, ir::shared::ScalarType::UINT64);
  ktime_var->set_builtin(ir::shared::BPFHelper::KTIME);

  // Add goid variable (if this is a go binary).
  if (language_ == ir::shared::Language::GOLANG) {
    auto* goid_var =
        AddVariable<ScalarVariable>(output_probe, kGOIDVarName, ir::shared::ScalarType::INT64);
    goid_var->set_builtin(ir::shared::BPFHelper::GOID);
  }
}

void Dwarvifier::AddEntryProbeVariables(ir::physical::Probe* output_probe) {
  if ((language_ == ir::shared::C || language_ == ir::shared::CPP)) {
    auto* parm_ptr_var =
        AddVariable<ScalarVariable>(output_probe, kParmPtrVarName, ir::shared::VOID_POINTER);
    parm_ptr_var->set_reg(ir::physical::Register::PARM_PTR);
  }
}

void Dwarvifier::AddRetProbeVariables(ir::physical::Probe* output_probe) {
  // Add return value variable for convenience.
  if ((language_ == ir::shared::C || language_ == ir::shared::CPP)) {
    auto* rc_var = AddVariable<ScalarVariable>(output_probe, kRCVarName, ir::shared::VOID_POINTER);
    rc_var->set_reg(ir::physical::Register::RC);

    auto* rc_ptr_var =
        AddVariable<ScalarVariable>(output_probe, kRCPtrVarName, ir::shared::VOID_POINTER);
    rc_ptr_var->set_reg(ir::physical::Register::RC_PTR);
  }
}

Status Dwarvifier::ProcessConstants(const ir::logical::Constant& constant,
                                    ir::physical::Probe* output_probe) {
  auto* var = output_probe->add_vars()->mutable_scalar_var();

  var->set_name(constant.name());
  var->set_type(constant.type());
  var->set_constant(constant.constant());

  return Status::OK();
}

Status Dwarvifier::ProcessVarExpr(const std::string& var_name, const ArgInfo& arg_info,
                                  const std::string& base_var,
                                  const std::vector<std::string_view>& components,
                                  ir::physical::Probe* output_probe) {
  using ir::shared::Language;

  // String for . operator (eg. my_struct.field)
  static constexpr std::string_view kDotStr = "_D_";

  // String for * operator (eg. (*my_struct).field)
  static constexpr std::string_view kDerefStr = "_X_";

  TypeInfo type_info = arg_info.type_info;
  int offset = arg_info.location.offset;
  std::string base = base_var;
  std::string name = var_name;

  // Note that we start processing at element [1], not [0], which was used to set the starting
  // state in the lines above.
  for (auto iter = components.begin() + 1; true; ++iter) {
    // If parent is a pointer, create a variable to dereference it.
    if (type_info.type == VarType::kPointer) {
      PL_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type,
                          VarTypeToProtoScalarType(type_info, language_));

      absl::StrAppend(&name, kDerefStr);

      auto* var = AddVariable<ScalarVariable>(output_probe, name, pb_type);
      var->mutable_memory()->set_base(base);
      var->mutable_memory()->set_offset(offset);

      // Reset base and offset.
      base = name;
      offset = 0;

      PL_ASSIGN_OR_RETURN(type_info, dwarf_reader_->DereferencePointerType(type_info.type_name));
    }

    // Stop iterating only after dereferencing any pointers one last time.
    if (iter == components.end()) {
      break;
    }

    std::string_view field_name = *iter;
    PL_ASSIGN_OR_RETURN(StructMemberInfo member_info,
                        dwarf_reader_->GetStructMemberInfo(type_info.type_name, field_name));
    offset += member_info.offset;
    type_info = std::move(member_info.type_info);
    absl::StrAppend(&name, kDotStr, field_name);
  }

  // Now we make the final variable.
  // Note that the very last created variable uses the original id.
  // This is important so that references in the original probe are maintained.

  ScalarVariable* var = nullptr;

  if (type_info.type == VarType::kBaseType) {
    PL_ASSIGN_OR_RETURN(ir::shared::ScalarType scalar_type,
                        VarTypeToProtoScalarType(type_info, language_));

    var = AddVariable<ScalarVariable>(output_probe, var_name, scalar_type);
  } else if (type_info.type == VarType::kStruct) {
    // Strings and byte arrays are special cases of structs, where we follow the pointer to the
    // data. Otherwise, just grab the raw data of the struct, and send it as a blob.
    if (language_ == Language::GOLANG && type_info.type_name == "string") {
      var = AddVariable<ScalarVariable>(output_probe, var_name, ir::shared::ScalarType::STRING);
    } else if (language_ == Language::GOLANG && type_info.type_name == "[]uint8") {
      var = AddVariable<ScalarVariable>(output_probe, var_name, ir::shared::ScalarType::BYTE_ARRAY);
    } else if (language_ == Language::GOLANG && type_info.type_name == "[]byte") {
      var = AddVariable<ScalarVariable>(output_probe, var_name, ir::shared::ScalarType::BYTE_ARRAY);
    } else {
      var =
          AddVariable<ScalarVariable>(output_probe, var_name, ir::shared::ScalarType::STRUCT_BLOB);

      // STRUCT_BLOB is special. It is the only type where we specify the size to get copied.
      PL_ASSIGN_OR_RETURN(uint64_t struct_byte_size,
                          dwarf_reader_->GetStructByteSize(type_info.type_name));
      var->mutable_memory()->set_size(struct_byte_size);
    }
  } else {
    return error::Internal("Expected struct or base type, but got type: $0", type_info.ToString());
  }

  DCHECK(var != nullptr);
  var->mutable_memory()->set_base(base);
  var->mutable_memory()->set_offset(offset);

  return Status::OK();
}

Status Dwarvifier::ProcessArgExpr(const ir::logical::Argument& arg,
                                  ir::physical::Probe* output_probe) {
  if (arg.expr().empty()) {
    return error::InvalidArgument("Argument '$0' expression cannot be empty", arg.id());
  }

  std::vector<std::string_view> components = absl::StrSplit(arg.expr(), ".");

  PL_ASSIGN_OR_RETURN(ArgInfo arg_info, GetArgInfo(args_map_, components.front()));

  switch (language_) {
    case ir::shared::GOLANG:
      return ProcessVarExpr(arg.id(), arg_info, kSPVarName, components, output_probe);
    case ir::shared::CPP:
    case ir::shared::C: {
      std::string base_var;

      switch (arg_info.location.loc_type) {
        case LocationType::kStack:
          base_var = kSPVarName;
          break;
        case LocationType::kRegister:
          base_var = kParmPtrVarName;
          break;
        default:
          return error::Internal("Unsupported argument LocationType $0",
                                 magic_enum::enum_name(arg_info.location.loc_type));
      }

      return ProcessVarExpr(arg.id(), arg_info, base_var, components, output_probe);
    }

    default:
      return error::Internal("Argument expressions not yet supported for language=$0",
                             magic_enum::enum_name(language_));
  }
}

Status Dwarvifier::ProcessRetValExpr(const ir::logical::ReturnValue& ret_val,
                                     ir::physical::Probe* output_probe) {
  if (ret_val.expr().empty()) {
    return error::InvalidArgument("ReturnValue '$0' expression cannot be empty", ret_val.id());
  }

  std::vector<std::string_view> components = absl::StrSplit(ret_val.expr(), ".");

  int index = -1;

  if (!absl::SimpleAtoi(components.front().substr(1), &index)) {
    return error::InvalidArgument(
        "ReturnValue '$0' expression invalid, first component must be `$$<index>`", ret_val.expr());
  }

  switch (language_) {
    case ir::shared::GOLANG: {
      // TODO(oazizi): Support named return variables.
      // Golang automatically names return variables ~r0, ~r1, etc.
      // However, it should be noted that the indexing includes function arguments.
      // For example Foo(a int, b int) (int, int) would have ~r2 and ~r3 as return variables.
      // One additional nuance is that the receiver, although an argument for dwarf purposes,
      // is not counted in the indexing.
      // For now, we throw the burden of finding the index to the user,
      // so if they want the first return argument above, they would have to specify and index of 2.
      // TODO(oazizi): Make indexing of return value based on number of return arguments only.
      std::string ret_val_name = absl::StrCat("~r", std::to_string(index));

      // Reset the first component.
      components.front() = ret_val_name;

      // Golang return values are really arguments located on the stack, so get the arg info.
      PL_ASSIGN_OR_RETURN(ArgInfo arg_info, GetArgInfo(args_map_, components.front()));

      return ProcessVarExpr(ret_val.id(), arg_info, kSPVarName, components, output_probe);
    }
    case ir::shared::CPP:
    case ir::shared::C: {
      if (index != 0) {
        return error::Internal("C/C++ only supports a single return value [index=$0].", index);
      }

      if (retval_info_.type_info.type == VarType::kVoid) {
        return error::Internal(
            "Attempting to process return variable for function with void return. Symbol=$0",
            output_probe->tracepoint().symbol());
      }

      DCHECK(retval_info_.type_info.type == VarType::kStruct ||
             retval_info_.type_info.type == VarType::kBaseType);

      std::string base_var;

      // The maximum size of a return value (in bytes) that will be returned directly through
      // registers, according to the System V ABI calling convention.
      // This is equivalent to two registers on a 64-bit machine.
      // Useful reference: https://wiki.osdev.org/System_V_ABI
      constexpr int kRegReturnBytesThreshold = 16;

      if (retval_info_.byte_size <= kRegReturnBytesThreshold) {
        // When the return value is small enough, the return value is passed directly
        // through a register that is accessed via PT_REGS_RC.
        // Copy the value onto the BPF stack, and set a pointer to it.
        base_var = kRCPtrVarName;
      } else {
        // When the return value doesn't fit in a register, the return value is a pointer to the
        // struct. That pointer is accessed via PT_REGS_RC.
        base_var = kRCVarName;
      }

      ArgInfo retval;
      retval.type_info = retval_info_.type_info;
      retval.location.loc_type = LocationType::kStack;
      retval.location.offset = 0;

      return ProcessVarExpr(ret_val.id(), retval, base_var, components, output_probe);
    }
    default:
      return error::Internal("Return expressions not yet supported for language=$0",
                             magic_enum::enum_name(language_));
  }
}

Status Dwarvifier::ProcessMapVal(const ir::logical::MapValue& map_val,
                                 ir::physical::Probe* output_probe) {
  // Find the map.
  auto map_iter = maps_.find(map_val.map_name());
  if (map_iter == maps_.end()) {
    return error::Internal("ProcessMapVal [probe=$0]: Reference to undeclared map: $1",
                           output_probe->name(), map_val.map_name());
  }
  auto* map = map_iter->second;

  // Find the map struct.
  std::string struct_type_name = StructTypeName(map_val.map_name());
  auto struct_iter = structs_.find(struct_type_name);
  if (struct_iter == structs_.end()) {
    return error::Internal("ProcessMapVal [probe=$0]: Reference to undeclared struct: $1",
                           output_probe->name(), struct_type_name);
  }
  auto* struct_decl = struct_iter->second;

  std::string map_var_name = absl::StrCat(map_val.map_name(), "_ptr");

  // Create the map variable.
  {
    auto* var = output_probe->add_vars()->mutable_map_var();
    var->set_name(map_var_name);
    var->set_type(map->value_type().struct_type());
    var->set_map_name(map_val.map_name());
    PL_ASSIGN_OR_RETURN(std::string key_var_name, BPFHelperVariableName(map_val.key()));
    var->set_key_variable_name(key_var_name);
  }

  // Unpack the map variable's members.
  int i = 0;
  for (const auto& value_id : map_val.value_ids()) {
    const auto& field = struct_decl->fields(i++);

    auto* var = AddVariable<MemberVariable>(output_probe, value_id, field.type());
    var->set_struct_base(map_var_name);
    var->set_is_struct_base_pointer(true);
    var->set_field(field.name());
  }

  return Status::OK();
}

Status Dwarvifier::ProcessFunctionLatency(const ir::shared::FunctionLatency& function_latency,
                                          ir::physical::Probe* output_probe) {
  auto* var = AddVariable<ScalarVariable>(output_probe, function_latency.id(),
                                          ir::shared::ScalarType::INT64);

  auto* expr = var->mutable_binary_expr();
  expr->set_op(ScalarVariable::BinaryExpression::SUB);
  expr->set_lhs(kKTimeVarName);
  expr->set_rhs(kStartKTimeNSVarName);

  // TODO(yzhao): Add more checks.

  return Status::OK();
}

Status Dwarvifier::GenerateMapValueStruct(const ir::logical::MapStashAction& stash_action_in,
                                          const std::string& struct_type_name,
                                          ir::physical::Program* output_program) {
  // TODO(oazizi): Check if struct already exists. If it does, make sure it is the same.

  auto* struct_decl = output_program->add_structs();
  struct_decl->set_name(struct_type_name);

  for (const auto& var_name : stash_action_in.value_variable_name()) {
    auto* struct_field = struct_decl->add_fields();
    struct_field->set_name(var_name);

    auto iter = variables_.find(var_name);
    if (iter == variables_.end()) {
      return error::Internal(
          "GenerateMapValueStruct [map_name=$0]: Reference to unknown variable: $1",
          stash_action_in.map_name(), var_name);
    }
    struct_field->set_type(iter->second);
  }

  structs_[struct_type_name] = struct_decl;
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
                                      ir::physical::Probe* output_probe,
                                      ir::physical::Program* output_program) {
  std::string variable_name = stash_action_in.map_name() + "_value";
  std::string struct_type_name = StructTypeName(stash_action_in.map_name());

  PL_RETURN_IF_ERROR(GenerateMapValueStruct(stash_action_in, struct_type_name, output_program));
  PL_RETURN_IF_ERROR(PopulateMapTypes(maps_, stash_action_in.map_name(), struct_type_name));

  auto* struct_var = output_probe->add_vars()->mutable_struct_var();
  struct_var->set_name(variable_name);
  struct_var->set_type(struct_type_name);

  for (const auto& f : stash_action_in.value_variable_name()) {
    auto* fa = struct_var->add_field_assignments();
    fa->set_field_name(f);
    fa->set_variable_name(f);
  }

  auto* stash_action_out = output_probe->add_map_stash_actions();
  stash_action_out->set_map_name(stash_action_in.map_name());

  PL_ASSIGN_OR_RETURN(std::string key_var_name, BPFHelperVariableName(stash_action_in.key()));
  stash_action_out->set_key_variable_name(key_var_name);
  stash_action_out->set_value_variable_name(variable_name);
  stash_action_out->mutable_cond()->CopyFrom(stash_action_in.cond());

  return Status::OK();
}

Status Dwarvifier::ProcessDeleteAction(const ir::logical::MapDeleteAction& delete_action_in,
                                       ir::physical::Probe* output_probe) {
  auto* delete_action_out = output_probe->add_map_delete_actions();
  delete_action_out->set_map_name(delete_action_in.map_name());

  PL_ASSIGN_OR_RETURN(std::string key_var_name, BPFHelperVariableName(delete_action_in.key()));
  delete_action_out->set_key_variable_name(key_var_name);

  return Status::OK();
}

Status Dwarvifier::GenerateOutputStruct(const ir::logical::OutputAction& output_action_in,
                                        const std::string& struct_type_name,
                                        ir::physical::Program* output_program) {
  // TODO(oazizi): Check if struct already exists. If it does, make sure it is the same.

  auto* struct_decl = output_program->add_structs();
  struct_decl->set_name(struct_type_name);

  for (const auto& f : implicit_columns_) {
    auto* struct_field = struct_decl->add_fields();
    struct_field->set_name(f);

    auto iter = variables_.find(f);
    if (iter == variables_.end()) {
      return error::Internal("GenerateOutputStruct [output=$0]: Reference to unknown variable $1",
                             output_action_in.output_name(), f);
    }

    struct_field->set_type(iter->second);
  }

  auto output_iter = outputs_.find(output_action_in.output_name());

  if (output_iter == outputs_.end()) {
    return error::InvalidArgument("Output '$0' was not defined", output_action_in.output_name());
  }

  const ir::physical::PerfBufferOutput* output = output_iter->second;

  if (output->fields_size() != output_action_in.variable_name_size()) {
    return error::InvalidArgument(
        "OutputAction to '$0' writes $1 variables, but the Output has $2 fields",
        output_action_in.output_name(), output_action_in.variable_name_size(),
        output->fields_size());
  }

  for (int i = 0; i < output_action_in.variable_name_size(); ++i) {
    auto* struct_field = struct_decl->add_fields();

    // Set the field name to the name in the Output.
    struct_field->set_name(output->fields(i));

    const std::string& var_name = output_action_in.variable_name(i);

    auto iter = variables_.find(var_name);
    if (iter == variables_.end()) {
      return error::Internal("GenerateOutputStruct [output=$0]: Reference to unknown variable $1",
                             output_action_in.output_name(), var_name);
    }
    struct_field->set_type(iter->second);
  }

  structs_[struct_type_name] = struct_decl;
  return Status::OK();
}

namespace {
Status PopulateOutputTypes(const std::map<std::string, ir::physical::PerfBufferOutput*>& outputs,
                           const std::string& output_name, const std::string& struct_type_name) {
  auto iter = outputs.find(output_name);
  if (iter == outputs.end()) {
    return error::Internal("Reference to undeclared map: $0", output_name);
  }

  auto* output = iter->second;

  if (!output->struct_type().empty() && output->struct_type() != struct_type_name) {
    return error::InvalidArgument("Output '$0' has output type '$1', which should be '$2'",
                                  output_name, output->struct_type(), struct_type_name);
  }

  output->set_struct_type(struct_type_name);

  return Status::OK();
}
}  // namespace

Status Dwarvifier::ProcessOutputAction(const ir::logical::OutputAction& output_action_in,
                                       ir::physical::Probe* output_probe,
                                       ir::physical::Program* output_program) {
  std::string variable_name = output_action_in.output_name() + "_value";
  std::string struct_type_name = StructTypeName(output_action_in.output_name());

  // Generate struct definition.
  PL_RETURN_IF_ERROR(GenerateOutputStruct(output_action_in, struct_type_name, output_program));

  // Generate an output definition.
  PL_RETURN_IF_ERROR(
      PopulateOutputTypes(outputs_, output_action_in.output_name(), struct_type_name));

  // Create and initialize a struct variable.
  auto* struct_var = output_probe->add_vars()->mutable_struct_var();
  struct_var->set_type(struct_type_name);
  struct_var->set_name(variable_name);

  // The Struct generated in above step is always the last element.
  const ir::physical::Struct& output_struct = *output_program->structs().rbegin();
  int struct_field_index = 0;

  for (const auto& f : implicit_columns_) {
    auto* fa = struct_var->add_field_assignments();
    fa->set_field_name(output_struct.fields(struct_field_index++).name());
    fa->set_variable_name(f);
  }

  for (const auto& f : output_action_in.variable_name()) {
    auto* fa = struct_var->add_field_assignments();
    fa->set_field_name(output_struct.fields(struct_field_index++).name());
    fa->set_variable_name(f);
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
