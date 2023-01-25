/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dwarvifier.h"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_replace.h>
#include <google/protobuf/repeated_field.h>

#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/obj_tools/go_syms.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/sharedpb/shared.pb.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::RepeatedPtrField;

using ::px::stirling::obj_tools::ArgInfo;
using ::px::stirling::obj_tools::LocationType;
using ::px::stirling::obj_tools::StructMemberInfo;
using ::px::stirling::obj_tools::StructSpecEntry;
using ::px::stirling::obj_tools::TypeInfo;
using ::px::stirling::obj_tools::VarType;

using ::px::stirling::dynamic_tracing::ir::physical::MapVariable;
using ::px::stirling::dynamic_tracing::ir::physical::PtrLenVariable;
using ::px::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::px::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::px::stirling::dynamic_tracing::ir::physical::Variable;

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
  Dwarvifier(obj_tools::DwarfReader* dwarf_reader, obj_tools::ElfReader* elf_reader,
             ir::shared::Language language);
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

  // Used by ProcessVarExpr() to handle Golang interface variables.
  // TODO(yzhao): Too many parameters, consider grouping into abstract type.
  Status ProcessGolangInterfaceExpr(const std::string& base, uint64_t offset,
                                    const TypeInfo& type_info, const std::string& var_name,
                                    ir::physical::Probe* output_probe);

  // Generates a ScalarVariable for a StrutBlob variable described by the input type info,
  // and located at the (base + offset) address.
  StatusOr<ScalarVariable> GenerateStructBlobVariable(uint8_t idx, const std::string& base,
                                                      uint64_t offset, const TypeInfo& type_info);

  // Used by ProcessVarExpr() to handle a Struct variable.
  Status ProcessStructBlob(const std::string& base, uint64_t offset, const TypeInfo& type_info,
                           const std::string& var_name, ir::physical::Probe* output_probe);

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

  // TVarType can be ScalarVariable, StructVariable, etc.
  template <typename TVarType>
  TVarType* AddVariable(ir::physical::Probe* probe, const std::string& name,
                        ir::shared::ScalarType type,
                        RepeatedPtrField<ir::physical::StructSpec> decoder = {});

  PtrLenVariable* AddPtrLenVariable(ir::physical::Probe* probe, const std::string& name,
                                    ir::shared::ScalarType type, const std::string& base,
                                    int ptr_offset, int len_offset);

  Status GenerateMapValueStruct(const ir::logical::MapStashAction& stash_action_in,
                                const std::string& struct_type_name,
                                ir::physical::Program* output_program);
  Status GenerateOutputStruct(const ir::logical::OutputAction& output_action_in,
                              const std::string& struct_type_name,
                              ir::physical::Program* output_program);

  std::map<std::string, ir::shared::Map*> maps_;
  std::map<std::string, ir::physical::PerfBufferOutput*> outputs_;
  std::map<std::string, ir::physical::Struct*> structs_;

  obj_tools::DwarfReader* dwarf_reader_ = nullptr;
  obj_tools::ElfReader* elf_reader_ = nullptr;

  std::map<std::string, obj_tools::ArgInfo> args_map_;
  obj_tools::RetValInfo retval_info_;

  // All defined variables.
  absl::flat_hash_map<std::string, ir::physical::Field> variables_;

  ir::shared::Language language_;
  std::vector<std::string_view> implicit_columns_;
};

// GeneratePhysicalProgram is the main entry point.
StatusOr<ir::physical::Program> GeneratePhysicalProgram(
    const ir::logical::TracepointDeployment& input, obj_tools::DwarfReader* dwarf_reader,
    obj_tools::ElfReader* elf_reader) {
  if (input.tracepoints_size() != 1) {
    return error::InvalidArgument("Right now only support exactly 1 Tracepoint, got '$0'",
                                  input.tracepoints_size());
  }

  ir::physical::Program output_program;

  output_program.mutable_deployment_spec()->CopyFrom(input.deployment_spec());
  output_program.set_language(input.tracepoints(0).program().language());

  for (const auto& input_tracepoint : input.tracepoints()) {
    // Transform tracepoint program.
    Dwarvifier dwarvifier(dwarf_reader, elf_reader, input_tracepoint.program().language());
    PX_RETURN_IF_ERROR(dwarvifier.Generate(input_tracepoint.program(), &output_program));
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
constexpr char kBPVarName[] = "bp_";
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
        // TODO(nserrino,oazizi): The width of this is architecture dependent. We should
        // properly implement this mapping based on the system architecture of the machine.
        {"uintptr", ir::shared::ScalarType::UINT64},
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
        {"long",               ir::shared::ScalarType::LONG},
        {"long unsigned int",      ir::shared::ScalarType::ULONG},
        {"long long",          ir::shared::ScalarType::LONGLONG},
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

  if (retval.type_info.type == VarType::kUnspecified) {
    return error::Internal("Argument=$0 type is not supported yet", arg_name);
  }

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

Dwarvifier::Dwarvifier(obj_tools::DwarfReader* dwarf_reader, obj_tools::ElfReader* elf_reader,
                       ir::shared::Language language)
    : dwarf_reader_(dwarf_reader), elf_reader_(elf_reader), language_(language) {
  implicit_columns_ = {kTGIDVarName, kTGIDStartTimeVarName, kKTimeVarName};
  if (language_ == ir::shared::Language::GOLANG) {
    implicit_columns_.push_back(kGOIDVarName);
  }
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
    PX_RETURN_IF_ERROR(GenerateProbe(probe, output_program));
  }

  return Status::OK();
}

template <typename TVarType>
TVarType* Dwarvifier::AddVariable(ir::physical::Probe* probe, const std::string& name,
                                  ir::shared::ScalarType type,
                                  RepeatedPtrField<ir::physical::StructSpec> decoder) {
  TVarType* var;

  if constexpr (std::is_same_v<TVarType, ScalarVariable>) {
    var = probe->add_vars()->mutable_scalar_var();
    var->set_type(type);
  } else if constexpr (std::is_same_v<TVarType, StructVariable>) {
    // TODO(yzhao): This branch is not used, consider removing.
    var = probe->add_vars()->mutable_struct_var();
  } else if constexpr (std::is_same_v<TVarType, MapVariable>) {
    // TODO(yzhao): This branch is not used, consider removing.
    var = probe->add_vars()->mutable_map_var();
  } else if constexpr (std::is_same_v<TVarType, PtrLenVariable>) {
    var = probe->add_vars()->mutable_ptr_len_var();
    var->set_type(type);
  } else {
    COMPILE_TIME_ASSERT(false, "Unsupported ir::physical::Variable type");
  }

  var->set_name(name);

  // UNKNOWN means that this is not a variable definition, therefore can skip this check below.
  if (type == ir::shared::ScalarType::UNKNOWN) {
    return var;
  }

  auto& v = variables_[name];
  v.set_name(name);
  v.set_type(type);

  // Decoder should be present if and only if type is STRUCT_BLOB.
  DCHECK_EQ(type == ir::shared::ScalarType::STRUCT_BLOB, !decoder.empty());
  v.mutable_blob_decoders()->CopyFrom(std::move(decoder));

  return var;
}

PtrLenVariable* Dwarvifier::AddPtrLenVariable(ir::physical::Probe* probe, const std::string& name,
                                              ir::shared::ScalarType type, const std::string& base,
                                              int ptr_offset, int len_offset) {
  std::string ptr_var_name = name + "__ptr";
  std::string len_var_name = name + "__len";

  auto* ptr_var =
      AddVariable<ScalarVariable>(probe, ptr_var_name, ir::shared::ScalarType::VOID_POINTER);
  ptr_var->mutable_memory()->set_base(base);
  ptr_var->mutable_memory()->set_offset(ptr_offset);

  auto* len_var = AddVariable<ScalarVariable>(probe, len_var_name, ir::shared::ScalarType::INT);
  len_var->mutable_memory()->set_base(base);
  len_var->mutable_memory()->set_offset(len_offset);

  auto* var = AddVariable<PtrLenVariable>(probe, name, type);
  var->set_ptr_var_name(ptr_var_name);
  var->set_len_var_name(len_var_name);

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
  if (dwarf_reader_ != nullptr) {
    PX_ASSIGN_OR_RETURN(args_map_,
                        dwarf_reader_->GetFunctionArgInfo(input_probe.tracepoint().symbol()));
    PX_ASSIGN_OR_RETURN(retval_info_,
                        dwarf_reader_->GetFunctionRetValInfo(input_probe.tracepoint().symbol()));
  }

  auto* p = output_program->add_probes();

  p->set_name(input_probe.name());

  PX_RETURN_IF_ERROR(ProcessTracepoint(input_probe.tracepoint(), p));
  AddSpecialVariables(p);

  for (auto& constant : input_probe.consts()) {
    PX_RETURN_IF_ERROR(ProcessConstants(constant, p));
  }

  for (const auto& arg : input_probe.args()) {
    PX_RETURN_IF_ERROR(ProcessArgExpr(arg, p));
  }

  for (const auto& ret_val : input_probe.ret_vals()) {
    PX_RETURN_IF_ERROR(ProcessRetValExpr(ret_val, p));
  }

  for (const auto& map_val : input_probe.map_vals()) {
    PX_RETURN_IF_ERROR(ProcessMapVal(map_val, p));
  }

  if (IsFunctionLatencySpecified(input_probe)) {
    PX_RETURN_IF_ERROR(ProcessFunctionLatency(input_probe.function_latency(), p));
  }

  for (const auto& stash_action : input_probe.map_stash_actions()) {
    PX_RETURN_IF_ERROR(ProcessStashAction(stash_action, p, output_program));
  }

  for (const auto& delete_action : input_probe.map_delete_actions()) {
    PX_RETURN_IF_ERROR(ProcessDeleteAction(delete_action, p));
  }

  for (const auto& output_action : input_probe.output_actions()) {
    PX_RETURN_IF_ERROR(ProcessOutputAction(output_action, p, output_program));
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
    parm_ptr_var->set_reg(ir::physical::Register::SYSV_AMD64_ARGS_PTR);
  }
  // TODO(oazizi): For Golang 1.17+, will need the following:
  //  auto* parm_ptr_var =
  //          AddVariable<ScalarVariable>(output_probe, kParmPtrVarName, ir::shared::VOID_POINTER);
  //  parm_ptr_var->set_reg(ir::physical::Register::GOLANG_ARGS_PTR);
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

namespace {

StatusOr<ir::physical::StructSpec> CreateStructSpecProto(
    const std::vector<StructSpecEntry>& struct_spec_entires, ir::shared::Language lang) {
  ir::physical::StructSpec struct_spec;

  for (const auto& e : struct_spec_entires) {
    PX_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type,
                        VarTypeToProtoScalarType(e.type_info, lang));

    auto* entry_proto = struct_spec.add_entries();
    entry_proto->set_offset(e.offset);
    entry_proto->set_size(e.size);
    entry_proto->set_type(pb_type);
    entry_proto->set_path(e.path);
  }

  return struct_spec;
}

bool IsGolangPointerType(std::string_view type_name) { return absl::StartsWith(type_name, "*"); }

constexpr char kGolangInterfaceTypeName[] = "runtime.iface";

// Returns a C variable name that corresponds to the input variable name.
std::string GetCVariableName(std::string_view name) {
  std::string substituted_name = absl::StrReplaceAll(name, {{".", "__"}, {"/", "___"}});
  // Remove any other chars that is not allowed in C variable substituted_name.
  substituted_name.erase(std::remove_if(substituted_name.begin(), substituted_name.end(),
                                        [](char c) { return !std::isalnum(c) && (c != '_'); }),
                         substituted_name.end());
  return substituted_name;
}

}  // namespace

Status Dwarvifier::ProcessGolangInterfaceExpr(const std::string& base, uint64_t offset,
                                              const TypeInfo& type_info,
                                              const std::string& var_name,
                                              ir::physical::Probe* output_probe) {
  // TODO(yzhao): Needs to:
  // * Add a struct block variable, plus its size in the outer scope before these if statements.
  // * For each concrete type, produces a ConditionalBlock that enclose all leaf fields
  //   of the type, and write it out.
  // * For each concrete type, produces a type ID and spec for the StructBlob.
  // * For each concrete type, writes type ID along with the rest of the data.
  // * Add a output action for the struct blob variable.

  constexpr char kIfaceTabSuffix[] = "_intf_tab";
  constexpr char kIfaceDataSuffix[] = "_intf_data";

  // Used to determine the implementation type.
  PX_ASSIGN_OR_RETURN(const StructMemberInfo tab_mem_info,
                      dwarf_reader_->GetStructMemberInfo(kGolangInterfaceTypeName,
                                                         llvm::dwarf::DW_TAG_structure_type, "tab",
                                                         llvm::dwarf::DW_TAG_member));

  std::string iface_tab_var_name = absl::StrCat(var_name, kIfaceTabSuffix);
  ir::physical::ScalarVariable* iface_tab_var =
      AddVariable<ScalarVariable>(output_probe, iface_tab_var_name, ir::shared::ScalarType::UINT64);

  iface_tab_var->mutable_memory()->set_base(base);
  iface_tab_var->mutable_memory()->set_offset(offset + tab_mem_info.offset);

  // Used to copy the content of the implementation variable.
  PX_ASSIGN_OR_RETURN(const StructMemberInfo data_mem_info,
                      dwarf_reader_->GetStructMemberInfo(kGolangInterfaceTypeName,
                                                         llvm::dwarf::DW_TAG_structure_type, "data",
                                                         llvm::dwarf::DW_TAG_member));

  std::string iface_data_var_name = absl::StrCat(var_name, kIfaceDataSuffix);
  ir::physical::ScalarVariable* iface_data_var = AddVariable<ScalarVariable>(
      output_probe, iface_data_var_name, ir::shared::ScalarType::VOID_POINTER);

  iface_data_var->mutable_memory()->set_base(base);
  iface_data_var->mutable_memory()->set_offset(offset + data_mem_info.offset);

  // TODO(yzhao): In case this is reused elsewhere, let Dwarvifier manage the return value, to avoid
  // duplicate computation.
  PX_ASSIGN_OR_RETURN(const auto intf_impl_map, obj_tools::ExtractGolangInterfaces(elf_reader_));

  auto iter = intf_impl_map.find(type_info.decl_type);

  if (iter == intf_impl_map.end()) {
    return error::Internal(
        "While processing variable '$0', failed to find the implementation types of interface=$1.",
        var_name, type_info.decl_type);
  }

  if (iter->second.size() + 1 > std::numeric_limits<int8_t>::max()) {
    return error::Internal("The number of implementation types '$0' exceeds limit '$1'",
                           iter->second.size(), std::numeric_limits<int8_t>::max());
  }

  RepeatedPtrField<ir::physical::StructSpec> struct_spec;

  // Insert the interface itself as the first decoder. By default, the interface itself is output,
  // if it's nil or the type could not be identified.
  PX_ASSIGN_OR_RETURN(std::vector<StructSpecEntry> struct_spec_entires,
                      dwarf_reader_->GetStructSpec(type_info.type_name));
  PX_ASSIGN_OR_RETURN(ir::physical::StructSpec struct_spec_proto,
                      CreateStructSpecProto(struct_spec_entires, language_));
  struct_spec.Add(std::move(struct_spec_proto));

  // The first decoder is the one for runtime.iface itself, so the index starts from 1.
  size_t decoder_idx = 1;

  for (const obj_tools::IntfImplTypeInfo& info : iter->second) {
    if (IsGolangPointerType(info.type_name)) {
      LOG(WARNING) << absl::Substitute(
          "Does not support pointer type as interface implementation type yet, "
          "interface=$0 impl_type=$1",
          type_info.decl_type, type_info.type_name);
      continue;
    }

    // TODO(yzhao): Need to add support to read base type, which can also implement error interface.
    // One such example is syscall.Errno.
    auto struct_spec_entires_or = dwarf_reader_->GetStructSpec(info.type_name);
    if (!struct_spec_entires_or.ok()) {
      continue;
    }
    auto struct_spec_proto_or =
        CreateStructSpecProto(struct_spec_entires_or.ConsumeValueOrDie(), language_);
    if (!struct_spec_proto_or.ok()) {
      continue;
    }
    auto struct_byte_size_or = dwarf_reader_->GetStructByteSize(type_info.type_name);
    if (!struct_byte_size_or.ok()) {
      continue;
    }

    std::string intf_tab_addr_constant_name =
        absl::StrCat(GetCVariableName(info.type_name), "_sym_addr", decoder_idx);

    auto* intf_tab_addr_constant = AddVariable<ScalarVariable>(
        output_probe, intf_tab_addr_constant_name, ir::shared::ScalarType::UINT64);
    intf_tab_addr_constant->set_constant(std::to_string(info.address));

    auto* cond_block = output_probe->add_cond_blocks();

    cond_block->mutable_cond()->set_op(ir::shared::Condition::EQUAL);
    cond_block->mutable_cond()->add_vars(iface_tab_var->name());
    cond_block->mutable_cond()->add_vars(intf_tab_addr_constant->name());

    struct_spec.Add(struct_spec_proto_or.ConsumeValueOrDie());

    auto* scalar_var = cond_block->add_vars()->mutable_scalar_var();

    // Assign this particular type to the output variable.
    scalar_var->set_name(var_name);
    scalar_var->set_type(ir::shared::ScalarType::STRUCT_BLOB);
    scalar_var->mutable_memory()->set_op(ir::physical::ASSIGN_ONLY);
    // Decoder index +1 to accommodate the implicit added runtime.iface.
    scalar_var->mutable_memory()->set_decoder_idx(decoder_idx);
    scalar_var->mutable_memory()->set_base(iface_data_var_name);
    scalar_var->mutable_memory()->set_offset(0);
    scalar_var->mutable_memory()->set_size(struct_byte_size_or.ValueOrDie());

    // Increment decoder index after each successfully-resolved implementation type.
    ++decoder_idx;
  }

  // NOTE: Logically this should appear before the ConditionalBlock generation code above.
  // However, because protobuf message has vars field before cond_blocks, this reordering does not
  // affect the processing order, i.e., the below code is generated before the ConditionalBlock.
  //
  // NOTE: It is more convenient to collect StructSpec in the above loop.
  auto* var = AddVariable<ScalarVariable>(
      output_probe, var_name, ir::shared::ScalarType::STRUCT_BLOB, std::move(struct_spec));
  var->mutable_memory()->set_op(ir::physical::DEFINE_ONLY);

  // Assign this particular type to the output variable.
  PX_ASSIGN_OR_RETURN(uint64_t struct_byte_size,
                      dwarf_reader_->GetStructByteSize(type_info.type_name));

  auto* scalar_var = output_probe->add_vars()->mutable_scalar_var();
  scalar_var->set_name(var_name);
  scalar_var->set_type(ir::shared::ScalarType::STRUCT_BLOB);
  scalar_var->mutable_memory()->set_op(ir::physical::ASSIGN_ONLY);
  // Decoder index +1 to accommodate the implicit added runtime.iface.
  scalar_var->mutable_memory()->set_decoder_idx(0);
  scalar_var->mutable_memory()->set_base(base);
  scalar_var->mutable_memory()->set_offset(offset);
  scalar_var->mutable_memory()->set_size(struct_byte_size);

  return Status::OK();
}

Status Dwarvifier::ProcessStructBlob(const std::string& base, uint64_t offset,
                                     const TypeInfo& type_info, const std::string& var_name,
                                     ir::physical::Probe* output_probe) {
  PX_ASSIGN_OR_RETURN(std::vector<StructSpecEntry> struct_spec_entires,
                      dwarf_reader_->GetStructSpec(type_info.type_name));
  PX_ASSIGN_OR_RETURN(ir::physical::StructSpec struct_spec_proto,
                      CreateStructSpecProto(struct_spec_entires, language_));

  // STRUCT_BLOB is special. It is the only type where we specify the size to get copied.
  PX_ASSIGN_OR_RETURN(uint64_t struct_byte_size,
                      dwarf_reader_->GetStructByteSize(type_info.type_name));

  RepeatedPtrField<ir::physical::StructSpec> struct_spec;
  struct_spec.Add(std::move(struct_spec_proto));

  auto* var = AddVariable<ScalarVariable>(
      output_probe, var_name, ir::shared::ScalarType::STRUCT_BLOB, std::move(struct_spec));
  // For StuctBlob with one single schema, the index is always 0.
  var->mutable_memory()->set_decoder_idx(0);
  var->mutable_memory()->set_base(base);
  var->mutable_memory()->set_offset(offset);
  var->mutable_memory()->set_size(struct_byte_size);
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

  if (dwarf_reader_ == nullptr) {
    return error::Internal(
        "Could not find debug symbols, but symbols required to evaluate variable $0", var_name);
  }

  TypeInfo type_info = arg_info.type_info;
  int offset = arg_info.location.offset;
  std::string base = base_var;
  std::string name = var_name;

  // Note that we start processing at element [1], not [0], which was used to set the starting
  // state in the lines above.
  for (auto iter = components.begin() + 1; true; ++iter) {
    // If parent is a pointer, create a variable to dereference it.
    if (type_info.type == VarType::kPointer) {
      PX_ASSIGN_OR_RETURN(ir::shared::ScalarType pb_type,
                          VarTypeToProtoScalarType(type_info, language_));

      absl::StrAppend(&name, kDerefStr);

      auto* var = AddVariable<ScalarVariable>(output_probe, name, pb_type);
      var->mutable_memory()->set_base(base);
      var->mutable_memory()->set_offset(offset);

      // Reset base and offset.
      base = name;
      offset = 0;

      PX_ASSIGN_OR_RETURN(type_info, dwarf_reader_->DereferencePointerType(type_info.type_name));
    }

    // Stop iterating only after dereferencing any pointers one last time.
    if (iter == components.end()) {
      break;
    }

    std::string_view field_name = *iter;
    PX_ASSIGN_OR_RETURN(
        StructMemberInfo member_info,
        dwarf_reader_->GetStructMemberInfo(type_info.type_name, llvm::dwarf::DW_TAG_structure_type,
                                           field_name, llvm::dwarf::DW_TAG_member));
    offset += member_info.offset;
    type_info = std::move(member_info.type_info);
    absl::StrAppend(&name, kDotStr, field_name);
  }

  // Now we make the final variable.
  // Note that the very last created variable uses the original id.
  // This is important so that references in the original probe are maintained.

  if (type_info.type == VarType::kBaseType) {
    PX_ASSIGN_OR_RETURN(ir::shared::ScalarType scalar_type,
                        VarTypeToProtoScalarType(type_info, language_));

    auto var = AddVariable<ScalarVariable>(output_probe, var_name, scalar_type);
    var->mutable_memory()->set_base(base);
    var->mutable_memory()->set_offset(offset);
  } else if (type_info.type == VarType::kStruct) {
    // Strings and byte arrays are special cases of structs, where we follow the pointer to the
    // data. Otherwise, just grab the raw data of the struct, and send it as a blob.
    if (language_ == Language::GOLANG && type_info.type_name == "string") {
      // A go string is essentially a pointer and a len:
      // type stringStruct struct {
      //   str unsafe.Pointer
      //   len int
      //}
      // TODO(oazizi): Get these offsets from dwarf_reader to make sure its more robust.
      constexpr int ptr_offset = 0;
      constexpr int len_offset = sizeof(void*);

      AddPtrLenVariable(output_probe, var_name, ir::shared::ScalarType::STRING, base,
                        offset + ptr_offset, offset + len_offset);
    } else if (language_ == Language::GOLANG &&
               (type_info.type_name == "[]uint8" || type_info.type_name == "[]byte")) {
      // A go array is essentially a pointer, a length, and a capacity:
      // type slice struct {
      //   array unsafe.Pointer
      //   len   int
      //   cap   int
      //}
      // TODO(oazizi): Get these offsets from dwarf_reader to make sure its more robust.
      constexpr int ptr_offset = 0;
      constexpr int len_offset = sizeof(void*);

      AddPtrLenVariable(output_probe, var_name, ir::shared::ScalarType::BYTE_ARRAY, base,
                        offset + ptr_offset, offset + len_offset);
    } else if (language_ == ir::shared::Language::GOLANG &&
               // Meaning that this variable is an interface.
               type_info.type_name == kGolangInterfaceTypeName) {
      PX_RETURN_IF_ERROR(
          ProcessGolangInterfaceExpr(base, offset, type_info, var_name, output_probe));
    } else {
      PX_RETURN_IF_ERROR(ProcessStructBlob(base, offset, type_info, var_name, output_probe));
    }
  } else {
    return error::Internal("Expected struct or base type, but got type: $0", type_info.ToString());
  }

  return Status::OK();
}

Status Dwarvifier::ProcessArgExpr(const ir::logical::Argument& arg,
                                  ir::physical::Probe* output_probe) {
  if (arg.expr().empty()) {
    return error::InvalidArgument("Argument '$0' expression cannot be empty", arg.id());
  }

  std::vector<std::string_view> components = absl::StrSplit(arg.expr(), ".");

  PX_ASSIGN_OR_RETURN(ArgInfo arg_info, GetArgInfo(args_map_, components.front()));

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
        case LocationType::kStackBP:
          base_var = kBPVarName;
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

namespace {
StatusOr<int> GolangReturnValueIndex(const std::map<std::string, obj_tools::ArgInfo>& args_map) {
  // Search for the first argument that has a name that fits the pattern ~rX, where X is a number.
  const std::string kRetValPrefix = "~r";

  auto iter = args_map.upper_bound(kRetValPrefix);
  if (iter == args_map.end() || !absl::StartsWith(iter->first, kRetValPrefix)) {
    return error::Internal("Could not find any return arguments");
  }

  std::string_view s = iter->first;
  s.remove_prefix(kRetValPrefix.size());

  int index = 0;
  if (!absl::SimpleAtoi(s, &index)) {
    return error::Internal("Could not extract return value index from $0", iter->first);
  }

  return index;
}
}  // namespace

Status Dwarvifier::ProcessRetValExpr(const ir::logical::ReturnValue& ret_val,
                                     ir::physical::Probe* output_probe) {
  if (ret_val.expr().empty()) {
    return error::InvalidArgument("ReturnValue '$0' expression cannot be empty", ret_val.id());
  }

  std::vector<std::string_view> components = absl::StrSplit(ret_val.expr(), ".");

  switch (language_) {
    case ir::shared::GOLANG: {
      // This represents the actualy return value being returned,
      // without sub-field accesses.
      std::string ret_val_name(components.front());

      // If the expression starts with '$', then we need to resolve the name from the index.
      // Otherwise, caller is using named return values.
      if (ret_val_name[0] == '$') {
        int index = -1;

        if (!absl::SimpleAtoi(ret_val_name.substr(1), &index)) {
          return error::InvalidArgument(
              "ReturnValue '$0' expression invalid, first component must be `$$<index>`",
              ret_val.expr());
        }

        // Golang automatically names return variables ~r0, ~r1, etc.
        // However, it should be noted that the indexing includes function arguments.
        // For example Foo(a int, b int) (int, int) would have ~r2 and ~r3 as return variables.
        // One additional nuance is that the receiver, although an argument for dwarf purposes,
        // is not counted in the indexing.
        // To address this problem, we search for the first ~r<n> that we can find,
        // then we apply the index offset to all indices from the user.
        PX_ASSIGN_OR_RETURN(int first_index, GolangReturnValueIndex(args_map_));
        index += first_index;

        ret_val_name = absl::StrCat("~r", std::to_string(index));
      }

      // Golang return values are really arguments located on the stack, so get the arg info.
      PX_ASSIGN_OR_RETURN(ArgInfo arg_info, GetArgInfo(args_map_, ret_val_name));

      return ProcessVarExpr(ret_val.id(), arg_info, kSPVarName, components, output_probe);
    }
    case ir::shared::CPP:
    case ir::shared::C: {
      if (!components.front().empty() && components.front() != "$0") {
        return error::Internal(
            "C/C++ only supports a single unnamed return value. Use empty specifier: \"\". "
            "Received $0",
            components.front());
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
    PX_ASSIGN_OR_RETURN(std::string key_var_name, BPFHelperVariableName(map_val.key()));
    var->set_key_variable_name(key_var_name);
  }

  // Unpack the map variable's members.
  int i = 0;
  for (const auto& value_id : map_val.value_ids()) {
    const auto& field = struct_decl->fields(i++);

    auto* var =
        AddVariable<ScalarVariable>(output_probe, value_id, field.type(), field.blob_decoders());

    auto* src = var->mutable_member();
    src->set_struct_base(map_var_name);
    src->set_is_struct_base_pointer(true);
    src->set_field(field.name());
  }

  return Status::OK();
}

Status Dwarvifier::ProcessFunctionLatency(const ir::shared::FunctionLatency& function_latency,
                                          ir::physical::Probe* output_probe) {
  auto* var = AddVariable<ScalarVariable>(output_probe, function_latency.id(),
                                          ir::shared::ScalarType::INT64);

  auto* expr = var->mutable_binary_expr();
  expr->set_op(ir::physical::BinaryExpression::SUB);
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

  for (const auto& var_name : stash_action_in.value_variable_names()) {
    auto iter = variables_.find(var_name);
    if (iter == variables_.end()) {
      return error::Internal(
          "GenerateMapValueStruct [map_name=$0]: Reference to unknown variable: $1",
          stash_action_in.map_name(), var_name);
    }

    auto* struct_field = struct_decl->add_fields();
    struct_field->CopyFrom(iter->second);
    DCHECK_EQ(struct_field->name(), var_name);
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
  std::string variable_name = absl::StrCat(stash_action_in.map_name(), "_value");
  std::string struct_type_name = StructTypeName(stash_action_in.map_name());

  PX_RETURN_IF_ERROR(GenerateMapValueStruct(stash_action_in, struct_type_name, output_program));
  PX_RETURN_IF_ERROR(PopulateMapTypes(maps_, stash_action_in.map_name(), struct_type_name));

  auto* struct_var = output_probe->add_vars()->mutable_struct_var();
  struct_var->set_name(variable_name);
  struct_var->set_type(struct_type_name);

  for (const auto& f : stash_action_in.value_variable_names()) {
    auto* fa = struct_var->add_field_assignments();
    fa->set_field_name(f);
    fa->set_variable_name(f);
  }

  auto* stash_action_out = output_probe->add_map_stash_actions();
  stash_action_out->set_map_name(stash_action_in.map_name());

  PX_ASSIGN_OR_RETURN(std::string key_var_name, BPFHelperVariableName(stash_action_in.key()));
  stash_action_out->set_key_variable_name(key_var_name);
  stash_action_out->set_value_variable_name(variable_name);
  stash_action_out->mutable_cond()->CopyFrom(stash_action_in.cond());

  return Status::OK();
}

Status Dwarvifier::ProcessDeleteAction(const ir::logical::MapDeleteAction& delete_action_in,
                                       ir::physical::Probe* output_probe) {
  auto* delete_action_out = output_probe->add_map_delete_actions();
  delete_action_out->set_map_name(delete_action_in.map_name());

  PX_ASSIGN_OR_RETURN(std::string key_var_name, BPFHelperVariableName(delete_action_in.key()));
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
    auto iter = variables_.find(f);
    if (iter == variables_.end()) {
      return error::Internal("GenerateOutputStruct [output=$0]: Reference to unknown variable $1",
                             output_action_in.output_name(), f);
    }

    auto* struct_field = struct_decl->add_fields();
    struct_field->CopyFrom(iter->second);
    DCHECK_EQ(struct_field->name(), f);
  }

  auto output_iter = outputs_.find(output_action_in.output_name());

  if (output_iter == outputs_.end()) {
    return error::InvalidArgument("Output '$0' was not defined", output_action_in.output_name());
  }

  const ir::physical::PerfBufferOutput* output = output_iter->second;

  if (output->fields_size() != output_action_in.variable_names_size()) {
    return error::InvalidArgument(
        "OutputAction to '$0' writes $1 variables, but the Output has $2 fields",
        output_action_in.output_name(), output_action_in.variable_names_size(),
        output->fields_size());
  }

  for (int i = 0; i < output_action_in.variable_names_size(); ++i) {
    const std::string& var_name = output_action_in.variable_names(i);

    auto iter = variables_.find(var_name);
    if (iter == variables_.end()) {
      return error::Internal("GenerateOutputStruct [output=$0]: Reference to unknown variable $1",
                             output_action_in.output_name(), var_name);
    }

    auto* struct_field = struct_decl->add_fields();
    struct_field->CopyFrom(iter->second);

    // Set the field name to the name in the Output.
    struct_field->set_name(output->fields(i));
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

// Returns the struct name associated with an Output or Map.
std::string DataBufferArrayName(std::string_view output_name) {
  return absl::StrCat(output_name, "_data_buffer_array");
}

}  // namespace

Status Dwarvifier::ProcessOutputAction(const ir::logical::OutputAction& output_action_in,
                                       ir::physical::Probe* output_probe,
                                       ir::physical::Program* output_program) {
  std::string struct_type_name = StructTypeName(output_action_in.output_name());
  std::string data_buffer_array_name = DataBufferArrayName(output_action_in.output_name());

  // Add data buffer array.
  ir::physical::PerCPUArray* data_buffer_array = output_program->add_arrays();
  data_buffer_array->set_name(data_buffer_array_name);
  data_buffer_array->mutable_type()->set_struct_type(struct_type_name);
  data_buffer_array->set_capacity(1);

  // Generate struct definition.
  PX_RETURN_IF_ERROR(GenerateOutputStruct(output_action_in, struct_type_name, output_program));

  // Generate an output definition.
  PX_RETURN_IF_ERROR(
      PopulateOutputTypes(outputs_, output_action_in.output_name(), struct_type_name));

  // The Struct generated in above step is always the last element.
  const ir::physical::Struct& output_struct = *output_program->structs().rbegin();

  // Output data.
  auto* output_action_out = output_probe->add_output_actions();

  output_action_out->set_perf_buffer_name(output_action_in.output_name());
  output_action_out->set_data_buffer_array_name(data_buffer_array->name());
  output_action_out->set_output_struct_name(output_struct.name());

  for (const auto& f : implicit_columns_) {
    output_action_out->add_variable_names(std::string(f));
  }

  for (const auto& f : output_action_in.variable_names()) {
    output_action_out->add_variable_names(std::string(f));
  }

  return Status::OK();
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
