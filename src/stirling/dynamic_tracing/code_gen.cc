#include "src/stirling/dynamic_tracing/code_gen.h"

#include <memory>
#include <utility>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/dynamic_tracing/types.h"
#include "src/stirling/obj_tools/elf_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;
using ::pl::stirling::dynamic_tracing::ir::physical::MapDeleteAction;
using ::pl::stirling::dynamic_tracing::ir::physical::MapStashAction;
using ::pl::stirling::dynamic_tracing::ir::physical::MemberVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::OutputAction;
using ::pl::stirling::dynamic_tracing::ir::physical::PerfBufferOutput;
using ::pl::stirling::dynamic_tracing::ir::physical::Probe;
using ::pl::stirling::dynamic_tracing::ir::physical::Program;
using ::pl::stirling::dynamic_tracing::ir::physical::Register;
using ::pl::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::Struct;
using ::pl::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::pl::stirling::dynamic_tracing::ir::physical::Variable;
using ::pl::stirling::dynamic_tracing::ir::shared::BPFHelper;
using ::pl::stirling::dynamic_tracing::ir::shared::Condition;
using ::pl::stirling::dynamic_tracing::ir::shared::Map;
using ::pl::stirling::dynamic_tracing::ir::shared::Printk;
using ::pl::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::pl::stirling::dynamic_tracing::ir::shared::TracePoint;
using ::pl::stirling::dynamic_tracing::ir::shared::VariableType;
using ::pl::stirling::elf_tools::ElfReader;

#define PB_ENUM_SENTINEL_SWITCH_CLAUSE                             \
  LOG(DFATAL) << "Cannot happen. Needed to avoid default clause."; \
  break

#define GCC_SWITCH_RETURN                                \
  LOG(DFATAL) << "Cannot happen. Needed for GCC build."; \
  return {}

namespace {

// clang-format off
const absl::flat_hash_map<ScalarType, std::string_view> kScalarTypeToCType = {
    {ScalarType::VOID_POINTER, "void*"},
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

    {ScalarType::STRING, "struct string"},
    {ScalarType::BYTE_ARRAY, "struct byte_array"},
};
// clang-format on

StatusOr<std::string_view> GetPrintFormatCode(ScalarType scalar_type) {
  static const absl::flat_hash_map<ScalarType, std::string_view> kScalarTypePrintfFormatCode = {
      {ScalarType::BOOL, "d"},
      {ScalarType::INT, "d"},
      {ScalarType::INT8, "d"},
      {ScalarType::INT16, "d"},
      {ScalarType::INT32, "d"},
      {ScalarType::INT64, "ld"},
      {ScalarType::UINT, "d"},
      {ScalarType::UINT8, "d"},
      {ScalarType::UINT16, "d"},
      {ScalarType::UINT32, "d"},
      {ScalarType::UINT64, "ld"},
      // BPF does not support %f or %lf, use llx to show hex representation.
      {ScalarType::FLOAT, "lx"},
      {ScalarType::DOUBLE, "llx"},
      {ScalarType::VOID_POINTER, "llx"},
  };

  auto iter = kScalarTypePrintfFormatCode.find(scalar_type);
  if (iter == kScalarTypePrintfFormatCode.end()) {
    return error::InvalidArgument("ScalarVariable type '$0' does not have format code",
                                  magic_enum::enum_name(scalar_type));
  }
  return iter->second;
}

// Accepts a program of physical IR, and produces BCC code. Additionally, keeps a list of metadata
// that can be used for other operations.
class BCCCodeGenerator {
 public:
  explicit BCCCodeGenerator(const ir::physical::Program& program) : program_(program) {
    VLOG(1) << "BCCCodeGenerator input: " << program.DebugString();
  }

  // Generates BCC code lines from the input program.
  StatusOr<std::vector<std::string>> GenerateCodeLines();

 private:
  // Generates the code for a physical probe.
  StatusOr<std::vector<std::string>> GenerateProbe(const Probe& probe);

  const ir::physical::Program& program_;

  // Map from Struct names to their definition.
  absl::flat_hash_map<std::string_view, const ir::physical::Struct*> structs_;
};

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

StatusOr<std::string> GenVariableType(const VariableType& var_type) {
  switch (var_type.type_oneof_case()) {
    case VariableType::TypeOneofCase::kScalar:
      return std::string(GenScalarType(var_type.scalar()));
    case VariableType::TypeOneofCase::kStructType:
      return absl::Substitute("struct $0", var_type.struct_type());
    case VariableType::TypeOneofCase::TYPE_ONEOF_NOT_SET:
      return error::InvalidArgument("Field type must be set, var_type: $0", var_type.DebugString());
  }
  GCC_SWITCH_RETURN;
}

std::string GenField(const Struct::Field& field) {
  return absl::Substitute("$0 $1;", GenScalarType(field.type()), field.name());
}

}  // namespace

StatusOr<std::vector<std::string>> GenStruct(const Struct& st, int member_indent_size) {
  std::vector<std::string> code_lines;

  code_lines.push_back(absl::Substitute("struct $0 {", st.name()));

  for (const auto& field : st.fields()) {
    code_lines.push_back(absl::StrCat(std::string(member_indent_size, ' '), GenField(field)));
  }

  // TODO(yzhao): Consider only add this attribute to structs that are for perf buffer output.
  // This is added for simplicity, to disable padding, so that the perf buffer polling code does
  // not need to deal with padding.
  code_lines.push_back("} __attribute__((packed, aligned(1)));");

  return code_lines;
}

namespace {

std::string GenRegister(const ScalarVariable& var) {
  switch (var.reg()) {
    case Register::SP:
      return absl::Substitute("$0 $1 = ($0)PT_REGS_SP(ctx);", GenScalarType(var.type()),
                              var.name());
    case Register::Register_INT_MIN_SENTINEL_DO_NOT_USE_:
    case Register::Register_INT_MAX_SENTINEL_DO_NOT_USE_:
      PB_ENUM_SENTINEL_SWITCH_CLAUSE;
  }
  GCC_SWITCH_RETURN;
}

// TODO(yzhao/oazizi): Consider making this a member variable to avoid passing language directly.
//                     Currently deferring this because it will break the tests.
StatusOr<std::vector<std::string>> GenStringMemoryVariable(
    const ScalarVariable& var, const ir::shared::BinarySpec::Language& language) {
  std::vector<std::string> code_lines;

  // TODO(oazizi): Move string variable into a BPF map so that the size can be increased.
  switch (language) {
    case ir::shared::BinarySpec_Language_GOLANG:
      code_lines.push_back(absl::Substitute("void* $0_ptr__;", var.name()));
      code_lines.push_back(absl::Substitute("bpf_probe_read(&$0_ptr__, sizeof(void*), $1 + $2);",
                                            var.name(), var.memory().base(),
                                            var.memory().offset()));

      code_lines.push_back(absl::Substitute("uint64_t $0_len__;", var.name()));
      code_lines.push_back(absl::Substitute("bpf_probe_read(&$0_len__, sizeof(uint64_t), $1 + $2);",
                                            var.name(), var.memory().base(),
                                            var.memory().offset() + sizeof(char*)));

      code_lines.push_back(absl::Substitute(
          "$0_len__ = ($0_len__ > MAX_STR_LEN) ? MAX_STR_LEN : $0_len__;", var.name()));
      code_lines.push_back(absl::Substitute(
          "$0_len__ = $0_len__ & MAX_STR_MASK; // Keep verifier happy.", var.name()));

      code_lines.push_back(absl::Substitute("$0 $1 = {};", GenScalarType(var.type()), var.name()));
      code_lines.push_back(absl::Substitute("$0.len = $0_len__;", var.name()));

      code_lines.push_back(
          "// Read one extra byte to avoid passing a size of 0 to bpf_probe_read(), which causes "
          "BPF verifier issues on kernel 4.14.");
      code_lines.push_back(
          absl::Substitute("bpf_probe_read($0.buf, $0.len + 1, $0_ptr__);", var.name()));

      return code_lines;
    default:
      return error::Internal("Strings are not yet supported for this language ($0)",
                             magic_enum::enum_name(language));
  }
}

// TODO(oazizi): Consolidate with GenStringMemoryVariable?
StatusOr<std::vector<std::string>> GenByteArrayMemoryVariable(
    const ScalarVariable& var, const ir::shared::BinarySpec::Language& language) {
  std::vector<std::string> code_lines;

  // TODO(oazizi): Move string variable into a BPF map so that the size can be increased.
  switch (language) {
    case ir::shared::BinarySpec_Language_GOLANG:
      code_lines.push_back(absl::Substitute("void* $0_ptr__;", var.name()));
      code_lines.push_back(absl::Substitute("bpf_probe_read(&$0_ptr__, sizeof(void*), $1 + $2);",
                                            var.name(), var.memory().base(),
                                            var.memory().offset()));

      code_lines.push_back(absl::Substitute("uint64_t $0_len__;", var.name()));
      code_lines.push_back(absl::Substitute("bpf_probe_read(&$0_len__, sizeof(uint64_t), $1 + $2);",
                                            var.name(), var.memory().base(),
                                            var.memory().offset() + sizeof(uint8_t*)));

      code_lines.push_back(absl::Substitute(
          "$0_len__ = ($0_len__ > MAX_BYTE_ARRAY_LEN) ? MAX_BYTE_ARRAY_LEN : $0_len__;",
          var.name()));
      code_lines.push_back(absl::Substitute(
          "$0_len__ = $0_len__ & MAX_BYTE_ARRAY_MASK; // Keep verifier happy.", var.name()));

      code_lines.push_back(absl::Substitute("$0 $1 = {};", GenScalarType(var.type()), var.name()));
      code_lines.push_back(absl::Substitute("$0.len = $0_len__;", var.name()));

      code_lines.push_back(
          "// Read one extra byte to avoid passing a size of 0 to bpf_probe_read(), which causes "
          "BPF verifier issues on kernel 4.14.");
      code_lines.push_back(
          absl::Substitute("bpf_probe_read($0.buf, $0.len + 1, $0_ptr__);", var.name()));

      return code_lines;
    default:
      return error::Internal("Strings are not yet supported for this language ($0)",
                             magic_enum::enum_name(language));
  }
}

std::vector<std::string> GenMemoryVariable(const ScalarVariable& var) {
  std::vector<std::string> code_lines;
  code_lines.push_back(absl::Substitute("$0 $1;", GenScalarType(var.type()), var.name()));
  code_lines.push_back(absl::Substitute("bpf_probe_read(&$0, sizeof($1), $2 + $3);", var.name(),
                                        GenScalarType(var.type()), var.memory().base(),
                                        var.memory().offset()));
  return code_lines;
}

std::string GenBPFHelper(const ScalarVariable& var) {
  static const absl::flat_hash_map<BPFHelper, std::string_view> kBPFHelpers = {
      // TODO(yzhao): Implement GOID.
      {BPFHelper::GOID, "pl_goid()"},
      {BPFHelper::TGID, "bpf_get_current_pid_tgid() >> 32"},
      {BPFHelper::TGID_PID, "bpf_get_current_pid_tgid()"},
      {BPFHelper::KTIME, "bpf_ktime_get_ns()"},
      {BPFHelper::TGID_START_TIME, "pl_tgid_start_time()"},
  };
  auto iter = kBPFHelpers.find(var.builtin());
  DCHECK(iter != kBPFHelpers.end());
  return absl::Substitute("$0 $1 = $2;", GenScalarType(var.type()), var.name(), iter->second);
}

std::string GenConstant(const ScalarVariable& var) {
  return absl::Substitute("const $0 $1 = $2;", GenScalarType(var.type()), var.name(),
                          var.constant());
}

std::string_view GenOp(ScalarVariable::BinaryExpression::Op op) {
  static absl::flat_hash_map<ScalarVariable::BinaryExpression::Op, std::string_view> kOpTxts = {
      {ScalarVariable::BinaryExpression::SUB, "-"},
  };
  DCHECK(kOpTxts.contains(op));
  return kOpTxts[op];
}

std::string GenBinaryExpression(const ScalarVariable& var) {
  const auto& expr = var.binary_expr();
  return absl::Substitute("$0 $1 = $2 $3 $4;", GenScalarType(var.type()), var.name(), expr.lhs(),
                          GenOp(expr.op()), expr.rhs());
}

}  // namespace

StatusOr<std::vector<std::string>> GenScalarVariable(
    const ScalarVariable& var, const ir::shared::BinarySpec::Language& language) {
  switch (var.address_oneof_case()) {
    case ScalarVariable::AddressOneofCase::kReg: {
      std::vector<std::string> code_lines = {GenRegister(var)};
      return code_lines;
    }
    case ScalarVariable::AddressOneofCase::kBuiltin: {
      std::vector<std::string> code_lines = {GenBPFHelper(var)};
      return code_lines;
    }
    case ScalarVariable::AddressOneofCase::kMemory:
      if (var.type() == ir::shared::ScalarType::STRING) {
        return GenStringMemoryVariable(var, language);
      } else if (var.type() == ir::shared::ScalarType::BYTE_ARRAY) {
        return GenByteArrayMemoryVariable(var, language);
      } else {
        return GenMemoryVariable(var);
      }
    case ScalarVariable::AddressOneofCase::kConstant: {
      std::vector<std::string> code_lines = {GenConstant(var)};
      return code_lines;
    }
    case ScalarVariable::AddressOneofCase::kBinaryExpr: {
      // TODO(yzhao): Check lhs rhs were defined.
      std::vector<std::string> code_lines = {GenBinaryExpression(var)};
      return code_lines;
    }
    case ScalarVariable::AddressOneofCase::ADDRESS_ONEOF_NOT_SET:
      return error::InvalidArgument("address_oneof must be set");
  }
  GCC_SWITCH_RETURN;
}

StatusOr<std::vector<std::string>> GenStructVariable(const Struct& st,
                                                     const StructVariable& st_var) {
  if (st.name() != st_var.type()) {
    return error::InvalidArgument("Names of the struct do not match, $0 vs. $1", st.name(),
                                  st_var.type());
  }

  std::vector<std::string> code_lines;

  code_lines.push_back(absl::Substitute("struct $0 $1 = {};", st.name(), st_var.name()));

  for (const auto& fa : st_var.field_assignments()) {
    code_lines.push_back(
        absl::Substitute("$0.$1 = $2;", st_var.name(), fa.field_name(), fa.variable_name()));
  }

  return code_lines;
}

StatusOr<std::vector<std::string>> GenCondition(const Condition& condition, std::string body) {
  switch (condition.op()) {
    case Condition::NIL: {
      std::vector<std::string> code_lines = {std::move(body)};
      return code_lines;
    }
    case Condition::EQUAL: {
      if (condition.vars_size() != 2) {
        return error::InvalidArgument("Expect 2 variables, got $0", condition.vars_size());
      }
      std::vector<std::string> code_lines = {
          absl::Substitute("if ($0 == $1) {", condition.vars(0), condition.vars(1)),
          std::move(body),
          "}",
      };
      return code_lines;
    }
    case ir::shared::Condition_Op_Condition_Op_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ir::shared::Condition_Op_Condition_Op_INT_MAX_SENTINEL_DO_NOT_USE_:
      PB_ENUM_SENTINEL_SWITCH_CLAUSE;
  }
  GCC_SWITCH_RETURN;
}

namespace {

void MoveBackStrVec(std::vector<std::string>&& src, std::vector<std::string>* dst) {
  dst->insert(dst->end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
}

}  // namespace

// TODO(yzhao): Swap order of expr and dst, to be consistent with MoveBackStrVec().
#define MOVE_BACK_STR_VEC(expr, dst)                           \
  PL_ASSIGN_OR_RETURN(std::vector<std::string> str_vec, expr); \
  MoveBackStrVec(std::move(str_vec), dst);

// TODO(yzhao): Wrap map stash action inside "{}" to avoid variable naming conflict.
//
// TODO(yzhao): Alternatively, leave map key as another Variable message (would be pre-generated
// as part of the physical IR).
StatusOr<std::vector<std::string>> GenMapStashAction(const MapStashAction& action) {
  std::string update_code_line =
      absl::Substitute("$0.update(&$1, &$2);", action.map_name(), action.key_variable_name(),
                       action.value_variable_name());
  return GenCondition(action.cond(), std::move(update_code_line));
}

std::string GenMapDeleteAction(const MapDeleteAction& action) {
  return absl::Substitute("$0.delete(&$1);", action.map_name(), action.key_variable_name());
}

std::string GenOutput(const PerfBufferOutput& output) {
  return absl::Substitute("BPF_PERF_OUTPUT($0);", output.name());
}

std::string GenOutputAction(const OutputAction& action) {
  return absl::Substitute("$0.perf_submit(ctx, &$1, sizeof($1));", action.perf_buffer_name(),
                          action.variable_name());
}

namespace {

StatusOr<std::string> GenScalarVarPrintk(
    const absl::flat_hash_map<std::string_view, const ScalarVariable*>& scalar_vars,
    const absl::flat_hash_map<std::string_view, const MemberVariable*>& member_vars,
    const Printk& printk) {
  auto iter1 = scalar_vars.find(printk.scalar());
  auto iter2 = member_vars.find(printk.scalar());
  if (iter1 == scalar_vars.end() && iter2 == member_vars.end()) {
    return error::InvalidArgument("Variable '$0' is not defined", printk.scalar());
  }

  // This wont happen, as the previous steps rejects duplicate variable names.
  // Added here for safety.
  if (iter1 != scalar_vars.end() && iter2 != member_vars.end()) {
    LOG(DFATAL) << absl::Substitute(
        "ScalarVariable '$0' is defined as ScalarVariable and "
        "MemberVariable",
        printk.scalar());
  }

  ScalarType type = ScalarType::BOOL;

  if (iter1 != scalar_vars.end()) {
    type = iter1->second->type();
  }

  if (iter2 != member_vars.end()) {
    type = iter2->second->type();
  }

  PL_ASSIGN_OR_RETURN(std::string_view format_code, GetPrintFormatCode(type));

  return absl::Substitute(R"(bpf_trace_printk("$0: %$1\n", $0);)", printk.scalar(), format_code);
}

StatusOr<std::string> GenPrintk(
    const absl::flat_hash_map<std::string_view, const ScalarVariable*>& scalar_vars,
    const absl::flat_hash_map<std::string_view, const MemberVariable*>& member_vars,
    const Printk& printk) {
  switch (printk.content_oneof_case()) {
    case Printk::ContentOneofCase::kText:
      return absl::Substitute(R"(bpf_trace_printk("$0\n");)", printk.text());
    case Printk::ContentOneofCase::kScalar:
      return GenScalarVarPrintk(scalar_vars, member_vars, printk);
    case Printk::ContentOneofCase::CONTENT_ONEOF_NOT_SET:
      PB_ENUM_SENTINEL_SWITCH_CLAUSE;
  }
  GCC_SWITCH_RETURN;
}

std::string GenMapVariable(const ir::physical::MapVariable& map_var) {
  return absl::Substitute("struct $0* $1 = $2.lookup(&$3);", map_var.type(), map_var.name(),
                          map_var.map_name(), map_var.key_variable_name());
}

std::vector<std::string> GenMemberVariable(const ir::physical::MemberVariable& var) {
  if (var.is_struct_base_pointer()) {
    // TODO(yzhao): We should set a correct default value here. Two options:
    // * Set global default based on the type.
    // * Let MemberVariable specify a default.
    return {
        absl::Substitute("if ($0 == NULL) { return 0; }", var.struct_base()),
        absl::Substitute("$0 $1 = $2->$3;", GenScalarType(var.type()), var.name(),
                         var.struct_base(), var.field()),
    };
  }
  return {absl::Substitute("$0 $1 = $2.$3;", GenScalarType(var.type()), var.name(),
                           var.struct_base(), var.field())};
}

Status CreateVarName(absl::flat_hash_set<std::string_view>* var_names, std::string_view var_name,
                     std::string_view var_desc) {
  if (var_names->contains(var_name)) {
    return error::InvalidArgument("$0 '$1' was already defined", var_desc, var_name);
  }
  var_names->insert(var_name);
  return Status::OK();
}

Status CheckVarExists(absl::flat_hash_set<std::string_view>* var_names, std::string_view var_name,
                      std::string_view context) {
  if (!var_names->contains(var_name)) {
    return error::InvalidArgument("variable name '$0' was not defined [context = $1]", var_name,
                                  context);
  }
  return Status::OK();
}

}  // namespace

StatusOr<std::vector<std::string>> BCCCodeGenerator::GenerateProbe(const Probe& probe) {
  if (probe.name().empty()) {
    return error::InvalidArgument("Probe's name cannot be empty");
  }

  std::vector<std::string> code_lines;

  code_lines.push_back(absl::Substitute("int $0(struct pt_regs* ctx) {", probe.name()));

  absl::flat_hash_set<std::string_view> var_names;
  absl::flat_hash_map<std::string_view, const ScalarVariable*> scalar_vars;
  absl::flat_hash_map<std::string_view, const MemberVariable*> member_vars;

  for (const auto& var : probe.vars()) {
    switch (var.var_oneof_case()) {
      case Variable::VarOneofCase::kScalarVar: {
        PL_RETURN_IF_ERROR(CreateVarName(&var_names, var.scalar_var().name(), "ScalarVariable"));
        scalar_vars[var.scalar_var().name()] = &var.scalar_var();
        MOVE_BACK_STR_VEC(GenScalarVariable(var.scalar_var(), program_.binary_spec().language()),
                          &code_lines);
        break;
      }
      case Variable::VarOneofCase::kMapVar: {
        PL_RETURN_IF_ERROR(CreateVarName(&var_names, var.map_var().name(), "MapVariable"));
        code_lines.push_back(GenMapVariable(var.map_var()));
        break;
      }
      case Variable::VarOneofCase::kMemberVar: {
        PL_RETURN_IF_ERROR(CreateVarName(&var_names, var.member_var().name(), "MemberVariable"));
        member_vars[var.member_var().name()] = &var.member_var();
        MoveBackStrVec(GenMemberVariable(var.member_var()), &code_lines);
        break;
      }
      case Variable::VarOneofCase::kStructVar: {
        PL_RETURN_IF_ERROR(CreateVarName(&var_names, var.struct_var().name(), "MapVariable"));

        const auto& st_var = var.struct_var();

        for (const auto& fa : st_var.field_assignments()) {
          PL_RETURN_IF_ERROR(CheckVarExists(
              &var_names, fa.variable_name(),
              absl::Substitute("StructVariable '$0' field assignment", st_var.name())));
          // TODO(yzhao): Check variable types as well.
        }

        auto iter = structs_.find(st_var.type());
        if (iter == structs_.end()) {
          return error::InvalidArgument("Struct '$0' referenced in variable '$1' was not defined",
                                        st_var.type(), st_var.name());
        }

        MOVE_BACK_STR_VEC(GenStructVariable(*iter->second, st_var), &code_lines);
        break;
      }
      case Variable::VarOneofCase::VAR_ONEOF_NOT_SET:
        return error::InvalidArgument("Variable is not set");
    }
  }

  for (const auto& action : probe.map_stash_actions()) {
    PL_RETURN_IF_ERROR(CheckVarExists(&var_names, action.key_variable_name(),
                                      absl::Substitute("BPF map '$0' key", action.map_name())));
    PL_RETURN_IF_ERROR(CheckVarExists(&var_names, action.value_variable_name(),
                                      absl::Substitute("BPF map '$0' value", action.map_name())));
    MOVE_BACK_STR_VEC(GenMapStashAction(action), &code_lines);
  }

  for (const auto& action : probe.map_delete_actions()) {
    PL_RETURN_IF_ERROR(CheckVarExists(&var_names, action.key_variable_name(),
                                      absl::Substitute("BPF map '$0' key", action.map_name())));
    code_lines.push_back(GenMapDeleteAction(action));
  }

  for (const auto& action : probe.output_actions()) {
    PL_RETURN_IF_ERROR(
        CheckVarExists(&var_names, action.variable_name(),
                       absl::Substitute("Perf buffer '$0'", action.perf_buffer_name())));
    code_lines.push_back(GenOutputAction(action));
  }

  for (const auto& printk : probe.printks()) {
    PL_ASSIGN_OR_RETURN(std::string code_line, GenPrintk(scalar_vars, member_vars, printk));
    code_lines.push_back(std::move(code_line));
  }

  code_lines.push_back("return 0;");
  code_lines.push_back("}");

  return code_lines;
}

namespace {

StatusOr<std::vector<std::string>> GenMap(const Map& map) {
  PL_ASSIGN_OR_RETURN(std::string key_code, GenVariableType(map.key_type()));
  PL_ASSIGN_OR_RETURN(std::string value_code, GenVariableType(map.value_type()));
  std::vector<std::string> code_lines = {
      absl::Substitute("BPF_HASH($0, $1, $2);", map.name(), key_code, value_code)};
  return code_lines;
}

std::vector<std::string> GenIncludes() {
  return {
      "#include <linux/ptrace.h>",
      // For struct task_struct.
      "#include <linux/sched.h>",
  };
}

std::vector<std::string> GenMacros() {
  return {
      "#ifndef __inline",
      "#ifdef SUPPORT_BPF2BPF_CALL",
      "#define __inline",
      "#else",
      "#define __inline inline __attribute__((__always_inline__))",
      "#endif",
      "#endif",
  };
}

std::vector<std::string> GenNsecToClock() {
  return {
      "static __inline uint64_t pl_nsec_to_clock_t(uint64_t x) {",
      "return div_u64(x, NSEC_PER_SEC / USER_HZ);",
      "}",
  };
}

std::vector<std::string> GenTGIDStartTime() {
  return {
      "static __inline uint64_t pl_tgid_start_time() {",
      "struct task_struct* task_group_leader = "
      "((struct task_struct*)bpf_get_current_task())->group_leader;",
      // Linux 5.5 renames the variable to start_boottime.
      "#if LINUX_VERSION_CODE >= 328960",
      "return pl_nsec_to_clock_t(task_group_leader->start_boottime);",
      "#else",
      "return pl_nsec_to_clock_t(task_group_leader->real_start_time);",
      "#endif",
      "}",
  };
}

// This requires the presence of a probe for GOID, which defines the BPF map, and update the map
// values.
std::vector<std::string> GenGOID() {
  // TODO(yzhao): This name should be hardcoded inside goid probe generation.
  const char kGOIDMap[] = "pid_goid_map";
  return {
      "static __inline int64_t pl_goid() {",
      "uint64_t current_pid_tgid = bpf_get_current_pid_tgid();",
      absl::Substitute(
          "const struct pid_goid_map_value_t* goid_ptr = $0.lookup(&current_pid_tgid);", kGOIDMap),
      "return (goid_ptr == NULL) ? -1 : goid_ptr->goid;",
      "}",
  };
}

std::vector<std::string> GenUtilFNs() {
  std::vector<std::string> code_lines;
  MoveBackStrVec(GenNsecToClock(), &code_lines);
  MoveBackStrVec(GenTGIDStartTime(), &code_lines);
  return code_lines;
}

std::vector<std::string> GenStringType() {
  return {
      absl::Substitute("#define MAX_STR_LEN ($0-sizeof(int64_t)-1)", kStructStringSize),
      absl::Substitute("#define MAX_STR_MASK ($0-1)", kStructStringSize),
      "struct string {",
      "  uint64_t len;",
      "  char buf[MAX_STR_LEN];",
      "  // To keep 4.14 kernel verifier happy, we copy an extra byte.",
      "  // Keep a dummy character to absorb this garbage.",
      "  char dummy;",
      "};",
  };
}

std::vector<std::string> GenByteArrayType() {
  return {
      absl::Substitute("#define MAX_BYTE_ARRAY_LEN ($0-sizeof(int64_t)-1)", kStructByteArraySize),
      absl::Substitute("#define MAX_BYTE_ARRAY_MASK ($0-1)", kStructByteArraySize),
      "struct byte_array {",
      "  uint64_t len;",
      "  uint8_t buf[MAX_BYTE_ARRAY_LEN];",
      "  // To keep 4.14 kernel verifier happy, we copy an extra byte.",
      "  // Keep a dummy character to absorb this garbage.",
      "  char dummy;",
      "};",
  };
}

std::vector<std::string> GenTypes() {
  std::vector<std::string> code_lines;
  MoveBackStrVec(GenStringType(), &code_lines);
  MoveBackStrVec(GenByteArrayType(), &code_lines);
  return code_lines;
}

StatusOr<std::vector<std::string>> BCCCodeGenerator::GenerateCodeLines() {
  std::vector<std::string> code_lines;

  MoveBackStrVec(GenIncludes(), &code_lines);
  MoveBackStrVec(GenMacros(), &code_lines);
  MoveBackStrVec(GenUtilFNs(), &code_lines);
  MoveBackStrVec(GenTypes(), &code_lines);

  for (const auto& st : program_.structs()) {
    MOVE_BACK_STR_VEC(GenStruct(st), &code_lines);
    structs_[st.name()] = &st;
  }

  for (const auto& map : program_.maps()) {
    if (map.key_type().type_oneof_case() == VariableType::TypeOneofCase::kStructType &&
        !structs_.contains(map.key_type().struct_type())) {
      return error::InvalidArgument("Struct key type '$0' referenced in map '$1' was not defined",
                                    map.key_type().struct_type(), map.name());
    }
    if (map.value_type().type_oneof_case() == VariableType::TypeOneofCase::kStructType &&
        !structs_.contains(map.value_type().struct_type())) {
      return error::InvalidArgument("Struct key type '$0' referenced in map '$1' was not defined",
                                    map.value_type().struct_type(), map.name());
    }
    MOVE_BACK_STR_VEC(GenMap(map), &code_lines);
  }

  if (program_.binary_spec().language() == ir::shared::BinarySpec_Language_GOLANG) {
    // goid() accesses BPF map.
    MoveBackStrVec(GenGOID(), &code_lines);
  }

  for (const auto& output : program_.outputs()) {
    code_lines.push_back(GenOutput(output));
  }

  for (const auto& probe : program_.probes()) {
    MOVE_BACK_STR_VEC(GenerateProbe(probe), &code_lines);
  }

  return code_lines;
}

}  // namespace

StatusOr<std::string> GenProgram(const Program& program) {
  BCCCodeGenerator generator(program);
  PL_ASSIGN_OR_RETURN(std::vector<std::string> code_lines, generator.GenerateCodeLines());
  return absl::StrJoin(code_lines, "\n");
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
