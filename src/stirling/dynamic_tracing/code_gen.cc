#include "src/stirling/dynamic_tracing/code_gen.h"

#include <utility>

#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::dynamictracingpb::BPFHelper;
using ::pl::stirling::dynamictracingpb::MapStashAction;
using ::pl::stirling::dynamictracingpb::Register;
using ::pl::stirling::dynamictracingpb::ScalarType;
using ::pl::stirling::dynamictracingpb::Struct;
using ::pl::stirling::dynamictracingpb::Variable;
using ::pl::stirling::dynamictracingpb::VariableType;

namespace {

StatusOr<std::string> GenScalarField(const Struct::Field& field) {
  switch (field.type().scalar()) {
    case ScalarType::INT32:
      return absl::Substitute("int32_t $0;", field.name());
    case ScalarType::INT64:
      return absl::Substitute("int64_t $0;", field.name());
    case ScalarType::DOUBLE:
      return absl::Substitute("double $0;", field.name());
    case ScalarType::STRING:
      return absl::Substitute("char* $0;", field.name());
    case ScalarType::VOID_POINTER:
      return absl::Substitute("void* $0;", field.name());
    case ScalarType::ScalarType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ScalarType::ScalarType_INT_MAX_SENTINEL_DO_NOT_USE_:
      DCHECK("Needed to avoid default clause");
      return {};
  }
  return error::InvalidArgument("Should never happen");
}

StatusOr<std::string> GenField(const Struct::Field& field) {
  switch (field.type().type_oneof_case()) {
    case VariableType::TypeOneofCase::kScalar:
      return GenScalarField(field);
    case VariableType::TypeOneofCase::kStructType:
      return absl::Substitute("struct $0 $1;", field.type().struct_type(), field.name());
    case VariableType::TypeOneofCase::TYPE_ONEOF_NOT_SET:
      return error::InvalidArgument("Field type must be set");
  }
  return error::InvalidArgument("Should never happen");
}

}  // namespace

StatusOr<std::string> GenStruct(const Struct& st, int member_indent_size) {
  DCHECK_GT(st.fields_size(), 0);

  std::string bcc_code;

  absl::StrAppend(&bcc_code, absl::Substitute("struct $0 {\n", st.name()));

  for (const auto& field : st.fields()) {
    PL_ASSIGN_OR_RETURN(std::string field_code, GenField(field));

    absl::StrAppend(&bcc_code, std::string(member_indent_size, ' '), field_code, "\n");
  }

  absl::StrAppend(&bcc_code, "};\n");

  return bcc_code;
}

namespace {

#define PB_ENUM_SENTINEL_SWITCH_CLAUSE                             \
  LOG(DFATAL) << "Cannot happen. Needed to avoid default clause."; \
  break

#define GCC_SWITCH_RETURN                                \
  LOG(DFATAL) << "Cannot happen. Needed for GCC build."; \
  return {}

std::string_view GenScalarType(ScalarType type) {
  constexpr auto kCTypes =
      MakeArray<std::string_view>("int32_t", "int64_t", "double", "char*", "void*");
  return kCTypes[static_cast<int>(type)];
}

std::string GenRegister(const Variable& var) {
  switch (var.reg()) {
    case Register::SP:
      return absl::Substitute("$0 $1 = PT_REGS_SP(ctx);", GenScalarType(var.val_type()),
                              var.name());
    case Register::Register_INT_MIN_SENTINEL_DO_NOT_USE_:
    case Register::Register_INT_MAX_SENTINEL_DO_NOT_USE_:
      PB_ENUM_SENTINEL_SWITCH_CLAUSE;
  }
  GCC_SWITCH_RETURN;
}

std::string GenMemoryVariable(const Variable& var) {
  constexpr char kMemVarTmpl[] =
      "$0 $1;\n"
      "bpf_probe_read(&$1, sizeof($0), $2 + $3);\n";
  return absl::Substitute(kMemVarTmpl, GenScalarType(var.val_type()), var.name(),
                          var.memory().base(), var.memory().offset());
}

}  // namespace

StatusOr<std::string> GenVariable(const Variable& var) {
  switch (var.address_oneof_case()) {
    case Variable::AddressOneofCase::kReg:
      return GenRegister(var);
    case Variable::AddressOneofCase::kMemory:
      return GenMemoryVariable(var);
    case Variable::AddressOneofCase::ADDRESS_ONEOF_NOT_SET:
      return error::InvalidArgument("address_oneof must be set");
  }
  GCC_SWITCH_RETURN;
}

namespace {

struct VarNameAndCode {
  std::string_view var_name;
  std::string_view code;
};

VarNameAndCode GenBPFHelper(BPFHelper builtin) {
  constexpr auto kBPFHelpers = MakeArray<std::string_view>(
      // TODO(yzhao): Implement GOID.
      "uint64_t goid = goid();\n", "uint32_t tgid = bpf_get_current_pid_tgid() >> 32;\n",
      "uint64_t tgid_pid = bpf_get_current_pid_tgid();\n");

  constexpr auto kVarNames = MakeArray<std::string_view>("goid", "tgid", "tgid_pid");

  auto idx = static_cast<size_t>(builtin);

  return VarNameAndCode{kVarNames[idx], kBPFHelpers[idx]};
}

}  // namespace

// TODO(yzhao): Wrap map stash action inside "{}" to avoid variable naming conflict.
//
// TODO(yzhao): Alternatively, leave map key as another Variable message (would be pre-generated
// as part of the physical IR).
std::vector<std::string> GenMapStashAction(const MapStashAction& action) {
  std::vector<std::string> code_lines;

  VarNameAndCode map_key = GenBPFHelper(action.builtin());
  code_lines.push_back(std::string(map_key.code));

  std::string map_update_code = absl::Substitute("$0.update(&$1, &$2);\n", action.map_name(),
                                                 map_key.var_name, action.variable_name());
  code_lines.push_back(std::move(map_update_code));

  return code_lines;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
