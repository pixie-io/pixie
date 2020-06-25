#include "src/stirling/dynamic_tracing/code_gen.h"

#include <utility>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dynamictracingpb::BPFHelper;
using ::pl::stirling::dynamictracingpb::MapStashAction;
using ::pl::stirling::dynamictracingpb::OutputAction;
using ::pl::stirling::dynamictracingpb::Register;
using ::pl::stirling::dynamictracingpb::ScalarType;
using ::pl::stirling::dynamictracingpb::ScalarVariable;
using ::pl::stirling::dynamictracingpb::Struct;
using ::pl::stirling::dynamictracingpb::StructVariable;
using ::pl::stirling::dynamictracingpb::VariableType;

namespace {

// clang-format off
const absl::flat_hash_map<ScalarType, std::string_view> kScalarTypeToCType = {
    {ScalarType::BOOL, "bool"},
    {ScalarType::INT, "int"},
    {ScalarType::INT8, "int8_t"},
    {ScalarType::INT16, "int16_t"},
    {ScalarType::INT32, "int32_t"},
    {ScalarType::INT64, "int64_t"},
    {ScalarType::UINT, "uint"},
    {ScalarType::UINT8, "uint8_t"},
    {ScalarType::UINT16, "uint16_t"},
    {ScalarType::UINT32, "uint32_t"},
    {ScalarType::UINT64, "uint64_t"},
    {ScalarType::FLOAT, "float"},
    {ScalarType::DOUBLE, "double"},
    {ScalarType::STRING, "char*"},
    {ScalarType::VOID_POINTER, "void*"},
};
// clang-format on

std::string_view GenScalarType(ScalarType type) {
  auto iter = kScalarTypeToCType.find(type);
  if (iter == kScalarTypeToCType.end()) {
    LOG(DFATAL) << absl::Substitute("Mapping to C-type not present for $0",
                                    magic_enum::enum_name(type));
    // Should never get here, but return "int" just in case.
    return "int";
  }
  return iter->second;
}

StatusOr<std::string> GenField(const Struct::Field& field) {
  switch (field.type().type_oneof_case()) {
    case VariableType::TypeOneofCase::kScalar:
      return absl::Substitute("$0 $1;", GenScalarType(field.type().scalar()), field.name());
    case VariableType::TypeOneofCase::kStructType:
      return absl::Substitute("struct $0 $1;", field.type().struct_type(), field.name());
    case VariableType::TypeOneofCase::TYPE_ONEOF_NOT_SET:
      return error::InvalidArgument("Field type must be set");
  }
  return error::InvalidArgument("Should never happen");
}

}  // namespace

StatusOr<std::vector<std::string>> GenStruct(const Struct& st, int member_indent_size) {
  DCHECK_GT(st.fields_size(), 0);

  std::vector<std::string> code_lines;

  code_lines.push_back(absl::Substitute("struct $0 {", st.name()));

  for (const auto& field : st.fields()) {
    PL_ASSIGN_OR_RETURN(std::string field_code, GenField(field));

    code_lines.push_back(absl::StrCat(std::string(member_indent_size, ' '), field_code));
  }

  code_lines.push_back("};");

  return code_lines;
}

namespace {

#define PB_ENUM_SENTINEL_SWITCH_CLAUSE                             \
  LOG(DFATAL) << "Cannot happen. Needed to avoid default clause."; \
  break

#define GCC_SWITCH_RETURN                                \
  LOG(DFATAL) << "Cannot happen. Needed for GCC build."; \
  return {}

std::vector<std::string> GenRegister(const ScalarVariable& var) {
  switch (var.reg()) {
    case Register::SP:
      return {
          absl::Substitute("$0 $1 = PT_REGS_SP(ctx);", GenScalarType(var.val_type()), var.name())};
    case Register::Register_INT_MIN_SENTINEL_DO_NOT_USE_:
    case Register::Register_INT_MAX_SENTINEL_DO_NOT_USE_:
      PB_ENUM_SENTINEL_SWITCH_CLAUSE;
  }
  GCC_SWITCH_RETURN;
}

std::vector<std::string> GenMemoryVariable(const ScalarVariable& var) {
  std::vector<std::string> code_lines;
  code_lines.push_back(absl::Substitute("$0 $1;", GenScalarType(var.val_type()), var.name()));
  code_lines.push_back(absl::Substitute("bpf_probe_read(&$0, sizeof($1), $2 + $3);", var.name(),
                                        GenScalarType(var.val_type()), var.memory().base(),
                                        var.memory().offset()));
  return code_lines;
}

std::vector<std::string> GenBPFHelper(const ScalarVariable& var) {
  constexpr auto kBPFHelpers = MakeArray<std::string_view>(
      // TODO(yzhao): Implement GOID.
      "goid()", "bpf_get_current_pid_tgid() >> 32", "bpf_get_current_pid_tgid()");
  int idx = static_cast<int>(var.builtin());
  return {
      absl::Substitute("$0 $1 = $2;", GenScalarType(var.val_type()), var.name(), kBPFHelpers[idx])};
}

}  // namespace

StatusOr<std::vector<std::string>> GenScalarVariable(const ScalarVariable& var) {
  switch (var.address_oneof_case()) {
    case ScalarVariable::AddressOneofCase::kReg:
      return GenRegister(var);
    case ScalarVariable::AddressOneofCase::kMemory:
      return GenMemoryVariable(var);
    case ScalarVariable::AddressOneofCase::kBuiltin:
      return GenBPFHelper(var);
    case ScalarVariable::AddressOneofCase::ADDRESS_ONEOF_NOT_SET:
      return error::InvalidArgument("address_oneof must be set");
  }
  GCC_SWITCH_RETURN;
}

StatusOr<std::vector<std::string>> GenStructVariable(
    const ::pl::stirling::dynamictracingpb::Struct& st,
    const ::pl::stirling::dynamictracingpb::StructVariable& st_var) {
  if (st.name() != st_var.struct_name()) {
    return error::InvalidArgument("Names of the struct do not match, $0 vs. $1", st.name(),
                                  st_var.struct_name());
  }
  if (st.fields_size() != st_var.variable_names_size()) {
    return error::InvalidArgument(
        "The number of struct fields and variables do not match, $0 vs. $1", st.fields_size(),
        st_var.variable_names_size());
  }

  std::vector<std::string> code_lines;

  code_lines.push_back(absl::Substitute("struct $0 $1 = {};", st.name(), st_var.name()));

  for (int i = 0; i < st.fields_size() && i < st_var.variable_names_size(); ++i) {
    const auto& var_name = st_var.variable_names(i);
    if (var_name.name_oneof_case() != StructVariable::VariableName::NameOneofCase::kName) {
      continue;
    }
    code_lines.push_back(absl::Substitute("$0.$1 = $2;", st_var.name(), st.fields(i).name(),
                                          st_var.variable_names(i).name()));
  }

  return code_lines;
}

// TODO(yzhao): Wrap map stash action inside "{}" to avoid variable naming conflict.
//
// TODO(yzhao): Alternatively, leave map key as another Variable message (would be pre-generated
// as part of the physical IR).
std::vector<std::string> GenMapStashAction(const MapStashAction& action) {
  return {absl::Substitute("$0.update(&$1, &$2);", action.map_name(), action.key_variable_name(),
                           action.value_variable_name())};
}

std::vector<std::string> GenOutputAction(const OutputAction& action) {
  return {absl::Substitute("$0.perf_submit(ctx, &$1, sizeof($1));", action.perf_buffer_name(),
                           action.variable_name())};
}

namespace {

void MoveBackStrVec(std::vector<std::string>&& src, std::vector<std::string>* dst) {
  dst->insert(dst->end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
}

}  // namespace

StatusOr<std::vector<std::string>> GenPhysicalProbe(
    const ::pl::stirling::dynamictracingpb::PhysicalProbe& probe) {
  if (probe.name().empty()) {
    return error::InvalidArgument("Probe's name cannot be empty");
  }

  std::vector<std::string> code_lines;

#define MOVE_BACK_STR_VEC(dst, expr)                           \
  PL_ASSIGN_OR_RETURN(std::vector<std::string> str_vec, expr); \
  MoveBackStrVec(std::move(str_vec), dst);

  absl::flat_hash_map<std::string_view, const Struct*> structs;

  // Defines structs first before the probe function body.
  //
  // TODO(yzhao): Struct defs might be deduped between multiple PhysicalProbe messages, and being
  // put before any probe function.
  for (const auto& st : probe.structs()) {
    MOVE_BACK_STR_VEC(&code_lines, GenStruct(st));
    structs[st.name()] = &st;
  }

  code_lines.push_back(absl::Substitute("int $0(struct pt_regs* ctx) {", probe.name()));

  absl::flat_hash_set<std::string_view> var_names;

  for (const auto& var : probe.vars()) {
    var_names.insert(var.name());
    MOVE_BACK_STR_VEC(&code_lines, GenScalarVariable(var));
  }

  for (const auto& var : probe.st_vars()) {
    if (!structs.contains(var.struct_name())) {
      return error::InvalidArgument("Struct name '$0' referenced in variable '$1' was not defined",
                                    var.struct_name(), var.name());
    }

    for (const auto& var_name : var.variable_names()) {
      if (var_name.name_oneof_case() != StructVariable::VariableName::NameOneofCase::kName) {
        continue;
      }
      if (!var_names.contains(var_name.name())) {
        return error::InvalidArgument(
            "variable name '$0' assigned to struct variable '$1' was not defined", var_name.name(),
            var.name());
      }
      // TODO(yzhao): Check variable types as well.
    }

    if (var_names.contains(var.name())) {
      return error::InvalidArgument("variable name '$0' was redefined", var.name());
    }

    var_names.insert(var.name());
    MOVE_BACK_STR_VEC(&code_lines, GenStructVariable(*structs[var.struct_name()], var));
  }

  for (const auto& action : probe.map_stash_actions()) {
    if (!var_names.contains(action.key_variable_name())) {
      return error::InvalidArgument(
          "variable name '$0' as the key pushed to BPF map '$1' was not defined",
          action.key_variable_name(), action.map_name());
    }
    if (!var_names.contains(action.value_variable_name())) {
      return error::InvalidArgument(
          "variable name '$0' as the value pushed to BPF map '$1' was not defined",
          action.value_variable_name(), action.map_name());
    }
    MoveBackStrVec(GenMapStashAction(action), &code_lines);
  }

  for (const auto& action : probe.output_actions()) {
    if (!var_names.contains(action.variable_name())) {
      return error::InvalidArgument(
          "variable name '$0' submitted to perf buffer '$1' was not defined",
          action.variable_name(), action.perf_buffer_name());
    }
    MoveBackStrVec(GenOutputAction(action), &code_lines);
  }

  code_lines.push_back("return 0;");
  code_lines.push_back("}");

  return code_lines;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
