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

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/code_gen.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/types.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::px::stirling::bpf_tools::BPFProbeAttachType;
using ::px::stirling::bpf_tools::UProbeSpec;
using ::px::stirling::dynamic_tracing::ir::physical::BinaryExpression;
using ::px::stirling::dynamic_tracing::ir::physical::Field;
using ::px::stirling::dynamic_tracing::ir::physical::MapDeleteAction;
using ::px::stirling::dynamic_tracing::ir::physical::MapStashAction;
using ::px::stirling::dynamic_tracing::ir::physical::PerCPUArray;
using ::px::stirling::dynamic_tracing::ir::physical::PerfBufferOutput;
using ::px::stirling::dynamic_tracing::ir::physical::PerfBufferOutputAction;
using ::px::stirling::dynamic_tracing::ir::physical::Probe;
using ::px::stirling::dynamic_tracing::ir::physical::Program;
using ::px::stirling::dynamic_tracing::ir::physical::PtrLenVariable;
using ::px::stirling::dynamic_tracing::ir::physical::Register;
using ::px::stirling::dynamic_tracing::ir::physical::ScalarVariable;
using ::px::stirling::dynamic_tracing::ir::physical::Struct;
using ::px::stirling::dynamic_tracing::ir::physical::StructVariable;
using ::px::stirling::dynamic_tracing::ir::physical::Variable;
using ::px::stirling::dynamic_tracing::ir::shared::BPFHelper;
using ::px::stirling::dynamic_tracing::ir::shared::Condition;
using ::px::stirling::dynamic_tracing::ir::shared::Map;
using ::px::stirling::dynamic_tracing::ir::shared::Printk;
using ::px::stirling::dynamic_tracing::ir::shared::ScalarType;
using ::px::stirling::dynamic_tracing::ir::shared::Tracepoint;
using ::px::stirling::dynamic_tracing::ir::shared::VariableType;
using ::px::stirling::obj_tools::ElfReader;

#define PB_ENUM_SENTINEL_SWITCH_CLAUSE                             \
  LOG(DFATAL) << "Cannot happen. Needed to avoid default clause."; \
  break

#define GCC_SWITCH_RETURN                                \
  LOG(DFATAL) << "Cannot happen. Needed for GCC build."; \
  return {}

namespace {

// NOLINTNEXTLINE: runtime/string
const std::string kStructString = absl::StrCat("struct blob", kStructStringSize);

// NOLINTNEXTLINE: runtime/string
const std::string kStructByteArray = absl::StrCat("struct blob", kStructByteArraySize);

// NOLINTNEXTLINE: runtime/string
const std::string kStructBlob = absl::StrCat("struct struct_blob", kStructBlobSize);

// clang-format off
const absl::flat_hash_map<ScalarType, std::string_view> kScalarTypeToCType = {
    {ScalarType::VOID_POINTER, "void*"},
    {ScalarType::BOOL, "bool"},

    {ScalarType::SHORT, "short int"},
    {ScalarType::USHORT, "unsigned short int"},
    {ScalarType::INT, "int"},
    {ScalarType::UINT, "unsigned int"},
    {ScalarType::LONG, "long"},
    {ScalarType::ULONG, "unsigned long"},
    {ScalarType::LONGLONG, "long long"},
    {ScalarType::ULONGLONG, "unsigned long long"},

    {ScalarType::INT8, "int8_t"},
    {ScalarType::INT16, "int16_t"},
    {ScalarType::INT32, "int32_t"},
    {ScalarType::INT64, "int64_t"},
    {ScalarType::UINT8, "uint8_t"},
    {ScalarType::UINT16, "uint16_t"},
    {ScalarType::UINT32, "uint32_t"},
    {ScalarType::UINT64, "uint64_t"},

    {ScalarType::CHAR, "char"},
    {ScalarType::UCHAR, "uchar"},

    {ScalarType::FLOAT, "float"},
    {ScalarType::DOUBLE, "double"},

    {ScalarType::STRING, kStructString},
    {ScalarType::BYTE_ARRAY, kStructByteArray},
    {ScalarType::STRUCT_BLOB, kStructBlob},
};
// clang-format on

StatusOr<std::string_view> GetPrintFormatCode(ScalarType scalar_type) {
  static const absl::flat_hash_map<ScalarType, std::string_view> kScalarTypePrintfFormatCode = {
      {ScalarType::BOOL, "d"},

      {ScalarType::SHORT, "d"},
      {ScalarType::USHORT, "u"},
      {ScalarType::INT, "d"},
      {ScalarType::UINT, "u"},
      {ScalarType::LONG, "ld"},
      {ScalarType::ULONG, "lu"},
      {ScalarType::LONGLONG, "lld"},
      {ScalarType::ULONGLONG, "llu"},

      {ScalarType::INT8, "d"},
      {ScalarType::INT16, "d"},
      {ScalarType::INT32, "d"},
      {ScalarType::INT64, "ld"},
      {ScalarType::UINT8, "u"},
      {ScalarType::UINT16, "u"},
      {ScalarType::UINT32, "u"},
      {ScalarType::UINT64, "lu"},

      {ScalarType::CHAR, "c"},
      {ScalarType::UCHAR, "u"},

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
  // Generates the code for defining a variable in BCC.
  Status GenVariable(const Variable& var,
                     const absl::flat_hash_map<std::string_view, const Variable*>& vars,
                     std::vector<std::string>* code_lines) const;

  StatusOr<std::vector<std::string>> GenerateConditionalBlock(
      const ir::physical::ConditionalBlock& cond_block) const;

  // Generates the code for a physical probe.
  StatusOr<std::vector<std::string>> GenerateProbe(const Probe& probe) const;

  const ir::physical::Program& program_;

  // Map from Struct names to their definition.
  absl::flat_hash_map<std::string_view, const ir::physical::Struct*> structs_;
};

// Returns the C type name of the input ScalarType.
std::string_view GetScalarTypeCName(ScalarType type) {
  auto iter = kScalarTypeToCType.find(type);
  if (iter == kScalarTypeToCType.end()) {
    LOG(DFATAL) << absl::Substitute("Mapping to C-type not present for $0", type);
    // Should never get here, but return "int" just in case.
    return "int";
  }
  return iter->second;
}

StatusOr<std::string> GenVariableType(const VariableType& var_type) {
  switch (var_type.type_oneof_case()) {
    case VariableType::TypeOneofCase::kScalar:
      return std::string(GetScalarTypeCName(var_type.scalar()));
    case VariableType::TypeOneofCase::kStructType:
      return absl::Substitute("struct $0", var_type.struct_type());
    case VariableType::TypeOneofCase::TYPE_ONEOF_NOT_SET:
      return error::InvalidArgument("Field type must be set, var_type: $0", var_type.DebugString());
  }
  GCC_SWITCH_RETURN;
}

std::string GenField(const Field& field) {
  return absl::Substitute("$0 $1;", GetScalarTypeCName(field.type()), field.name());
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
  std::string_view type = GetScalarTypeCName(var.type());

  switch (var.reg()) {
    case Register::SP:
      return absl::Substitute("$0 $1 = ($0)PT_REGS_SP(ctx);", type, var.name());
    case Register::BP:
      return absl::Substitute("$0 $1 = ($0)PT_REGS_FP(ctx);", type, var.name());
    case Register::RC:
      return absl::Substitute("$0 $1 = ($0)PT_REGS_RC(ctx);", type, var.name());
    case Register::RC_PTR:
      // In the System V AMD64 ABI, a return value less than 16B in size is held in registers.
      // The first half is stored in rax, while the second half (if needed), is stored in rdx.
      // Copy the register values onto the BPF stack and return a pointer to the return value.
      return absl::Substitute(
          "uint64_t rc___[2];"
          "rc___[0] = ctx->ax;"
          "rc___[1] = ctx->dx;"
          "void* $0 = &rc___;",
          var.name());
    case Register::RAX:
      return absl::Substitute("$0 $1 = ($0)ctx->ax;", type, var.name());
    case Register::RBX:
      return absl::Substitute("$0 $1 = ($0)ctx->bx;", type, var.name());
    case Register::RCX:
      return absl::Substitute("$0 $1 = ($0)ctx->cx;", type, var.name());
    case Register::RDX:
      return absl::Substitute("$0 $1 = ($0)ctx->dx;", type, var.name());
    case Register::RDI:
      return absl::Substitute("$0 $1 = ($0)ctx->di;", type, var.name());
    case Register::RSI:
      return absl::Substitute("$0 $1 = ($0)ctx->si;", type, var.name());
    case Register::R8:
      return absl::Substitute("$0 $1 = ($0)ctx->r8;", type, var.name());
    case Register::R9:
      return absl::Substitute("$0 $1 = ($0)ctx->r9;", type, var.name());
    case Register::R10:
      return absl::Substitute("$0 $1 = ($0)ctx->r10;", type, var.name());
    case Register::R11:
      return absl::Substitute("$0 $1 = ($0)ctx->r11;", type, var.name());
    case Register::R12:
      return absl::Substitute("$0 $1 = ($0)ctx->r12;", type, var.name());
    case Register::R13:
      return absl::Substitute("$0 $1 = ($0)ctx->r13;", type, var.name());
    case Register::R14:
      return absl::Substitute("$0 $1 = ($0)ctx->r14;", type, var.name());
    case Register::R15:
      return absl::Substitute("$0 $1 = ($0)ctx->r15;", type, var.name());
    case Register::SYSV_AMD64_ARGS_PTR:
      // In the System V AMD64 ABI, there are 6 registers dedicated to passing arguments.
      // Small arguments may be passed through these registers.
      // Copy the register values onto the BPF stack and return a pointer to the return value.
      return absl::Substitute(
          "uint64_t parm___[6];"
          "parm___[0] = ctx->di;"
          "parm___[1] = ctx->si;"
          "parm___[2] = ctx->dx;"
          "parm___[3] = ctx->cx;"
          "parm___[4] = ctx->r8;"
          "parm___[5] = ctx->r9;"
          "void* $0 = &parm___;",
          var.name());
    case Register::GOLANG_ARGS_PTR:
      // In the new Golang ABI, there are 9 registers dedicated to passing arguments.
      // Copy the register values onto the BPF stack and return a pointer to the return value.
      return absl::Substitute(
          "uint64_t parm___[9];"
          "parm___[0] = ctx->ax;"
          "parm___[1] = ctx->bx;"
          "parm___[2] = ctx->cx;"
          "parm___[3] = ctx->di;"
          "parm___[4] = ctx->si;"
          "parm___[5] = ctx->r8;"
          "parm___[6] = ctx->r9;"
          "parm___[7] = ctx->r10;"
          "parm___[8] = ctx->r11;"
          "void* $0 = &parm___;",
          var.name());
    default:
      LOG(DFATAL) << absl::Substitute("Unsupported type: $0", type);
      return "";
  }
}

// Generate a variable that is fundamentally a pointer and a length (e.g. strings and arrays).
StatusOr<std::vector<std::string>> GenPtrLenVariable(const PtrLenVariable& var) {
  std::vector<std::string> code_lines;

  size_t size = 0;
  if (var.type() == ir::shared::ScalarType::STRING) {
    size = kStructStringSize;
  } else if (var.type() == ir::shared::ScalarType::BYTE_ARRAY) {
    size = kStructByteArraySize;
  } else {
    return error::Internal("GenPtrLenVariable $0 received an unsupported type: $1",
                           var.ShortDebugString(), ScalarType_Name(var.type()));
  }

  // Make sure we don't overrun the buffer by capping the length (also required for verifier).
  // Below, we want the size of blobXX->buf. We can do that with this trick:
  //   sizeof(((struct blobXX)0)->buf)
  // This trick won't cause a null dereference because it's just for the compiler
  // to find the size of the member; there is no real data access at run-time.
  // See https://stackoverflow.com/questions/3553296/sizeof-single-struct-member-in-c
  std::string sizeof_struct_blob = absl::Substitute("sizeof(((struct blob$0*)0)->buf)", size);

  code_lines.push_back(absl::Substitute("$0 $1 = {};", GetScalarTypeCName(var.type()), var.name()));
  code_lines.push_back(absl::Substitute("$0.truncated = $1 > $2;", var.name(), var.len_var_name(),
                                        sizeof_struct_blob));
  code_lines.push_back(absl::Substitute("$0.len = $0.truncated ? $2 : $1;", var.name(),
                                        var.len_var_name(), sizeof_struct_blob));

  code_lines.push_back("// Some black magic to keep BPF verifier of older kernels happy.");
  code_lines.push_back(absl::Substitute("size_t $0_len_minus_1 = $0.len - 1;", var.name()));
  code_lines.push_back(
      absl::Substitute("asm volatile(\"\" : \"+r\"($0_len_minus_1) :);", var.name()));
  code_lines.push_back(
      absl::Substitute("if ($0_len_minus_1 < $1) {", var.name(), sizeof_struct_blob));
  code_lines.push_back(absl::Substitute("  bpf_probe_read($0.buf, $0_len_minus_1 + 1, $1);",
                                        var.name(), var.ptr_var_name()));
  code_lines.push_back("}");

  return code_lines;
}

// Generate code lines for a STRUCT_BLOB variable.
std::vector<std::string> GenStructBlobMemoryVariable(const ScalarVariable& var) {
  // TODO(oazizi): Refactor the max size parameter.
  size_t size = std::min<size_t>(var.memory().size(), kStructBlobSize - sizeof(uint64_t) - 1);

  if (size < var.memory().size()) {
    LOG(WARNING) << absl::Substitute(
        "Not enough bytes to transmit variable. Truncating. [var_name=$0 original_size=$1 "
        "truncated_size=$2]",
        var.name(), var.memory().size(), size);
  }

  std::vector<std::string> code_lines;
  // Note that we initialize the variable with `= {}` to keep BPF happy.
  // This should always be valid since the variable will always be a struct and never a base type.
  if (var.memory().op() != ir::physical::ASSIGN_ONLY) {
    code_lines.push_back(
        absl::Substitute("$0 $1 = {};", GetScalarTypeCName(var.type()), var.name()));
  }

  if (var.memory().op() != ir::physical::DEFINE_ONLY) {
    code_lines.push_back(absl::Substitute("$0.len = $1;", var.name(), size));
    code_lines.push_back(
        absl::Substitute("$0.decoder_idx = $1;", var.name(), var.memory().decoder_idx()));
    code_lines.push_back(absl::Substitute("bpf_probe_read(&$0.buf, $1, $2 + $3);", var.name(), size,
                                          var.memory().base(), var.memory().offset()));
  }

  return code_lines;
}

// Returns code lines for producing a "simple" memory variable. These types are mapped to native
// C scalar types in kScalarTypeToCType.
std::vector<std::string> GenMemoryVariable(const ScalarVariable& var) {
  std::vector<std::string> code_lines;
  code_lines.push_back(absl::Substitute("$0 $1;", GetScalarTypeCName(var.type()), var.name()));
  code_lines.push_back(absl::Substitute("bpf_probe_read(&$0, sizeof($1), $2 + $3);", var.name(),
                                        GetScalarTypeCName(var.type()), var.memory().base(),
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
  return absl::Substitute("$0 $1 = $2;", GetScalarTypeCName(var.type()), var.name(), iter->second);
}

std::string GenConstant(const ScalarVariable& var) {
  return absl::Substitute("$0 $1 = $2;", GetScalarTypeCName(var.type()), var.name(),
                          var.constant());
}

std::string_view GenOp(BinaryExpression::Op op) {
  static absl::flat_hash_map<BinaryExpression::Op, std::string_view> kOpTxts = {
      {BinaryExpression::SUB, "-"},
  };
  DCHECK(kOpTxts.contains(op));
  return kOpTxts[op];
}

std::string GenBinaryExpression(const ScalarVariable& var) {
  const auto& expr = var.binary_expr();
  return absl::Substitute("$0 $1 = $2 $3 $4;", GetScalarTypeCName(var.type()), var.name(),
                          expr.lhs(), GenOp(expr.op()), expr.rhs());
}

std::vector<std::string> GenMemberExpression(const ScalarVariable& var) {
  const auto& expr = var.member();
  if (expr.is_struct_base_pointer()) {
    // TODO(yzhao): We should set a correct default value here. Two options:
    // * Set global default based on the type.
    // * Let MemberVariable specify a default.
    return {
        absl::Substitute("if ($0 == NULL) { return 0; }", expr.struct_base()),
        absl::Substitute("$0 $1 = $2->$3;", GetScalarTypeCName(var.type()), var.name(),
                         expr.struct_base(), expr.field()),
    };
  }
  return {absl::Substitute("$0 $1 = $2.$3;", GetScalarTypeCName(var.type()), var.name(),
                           expr.struct_base(), expr.field())};
}

}  // namespace

StatusOr<std::vector<std::string>> GenScalarVariable(const ScalarVariable& var) {
  switch (var.src_expr_oneof_case()) {
    case ScalarVariable::SrcExprOneofCase::kReg: {
      std::vector<std::string> code_lines = {GenRegister(var)};
      return code_lines;
    }
    case ScalarVariable::SrcExprOneofCase::kBuiltin: {
      std::vector<std::string> code_lines = {GenBPFHelper(var)};
      return code_lines;
    }
    case ScalarVariable::SrcExprOneofCase::kMemory:
      if (var.type() == ir::shared::ScalarType::STRING) {
        return error::Internal("Using STRING with ScalarVariable is no longer supported.");
      } else if (var.type() == ir::shared::ScalarType::BYTE_ARRAY) {
        return error::Internal("Using BYTE_ARRAY with ScalarVariable is no longer supported.");
      } else if (var.type() == ir::shared::ScalarType::STRUCT_BLOB) {
        return GenStructBlobMemoryVariable(var);
      } else {
        return GenMemoryVariable(var);
      }
    case ScalarVariable::SrcExprOneofCase::kConstant: {
      std::vector<std::string> code_lines = {GenConstant(var)};
      return code_lines;
    }
    case ScalarVariable::SrcExprOneofCase::kBinaryExpr: {
      // TODO(yzhao): Check lhs rhs were defined.
      std::vector<std::string> code_lines = {GenBinaryExpression(var)};
      return code_lines;
    }
    case ScalarVariable::SrcExprOneofCase::kMember: {
      // TODO(yzhao): Check lhs rhs were defined.
      std::vector<std::string> code_lines = GenMemberExpression(var);
      return code_lines;
    }
    case ScalarVariable::SrcExprOneofCase::SRC_EXPR_ONEOF_NOT_SET:
      return error::InvalidArgument("ScalarVariable.address_oneof must be set, got: $0",
                                    var.DebugString());
  }
  GCC_SWITCH_RETURN;
}

StatusOr<std::vector<std::string>> GenStructVariable(const StructVariable& st_var) {
  std::vector<std::string> code_lines;

  if (st_var.op() != ir::physical::ASSIGN_ONLY) {
    code_lines.push_back(absl::Substitute("struct $0 $1 = {};", st_var.type(), st_var.name()));
  }

  constexpr char kDot[] = ".";
  constexpr char kArrow[] = "->";

  const char* access_operator = st_var.is_pointer() ? kArrow : kDot;

  if (st_var.op() != ir::physical::DEFINE_ONLY) {
    for (const auto& fa : st_var.field_assignments()) {
      switch (fa.value_oneof_case()) {
        case StructVariable::FieldAssignment::kVariableName:
          code_lines.push_back(absl::Substitute("$0$1$2 = $3;", st_var.name(), access_operator,
                                                fa.field_name(), fa.variable_name()));
          break;
        case StructVariable::FieldAssignment::kValue:
          code_lines.push_back(absl::Substitute("$0$1$2 = $3;", st_var.name(), access_operator,
                                                fa.field_name(), fa.value()));
          break;
        case StructVariable::FieldAssignment::VALUE_ONEOF_NOT_SET:
          return error::InvalidArgument(
              "FieldAssignment of StructVariable '$0' does not have value", st_var.name());
      }
    }
  }

  return code_lines;
}

StatusOr<std::vector<std::string>> GenCondition(const Condition& condition,
                                                std::vector<std::string> body) {
  switch (condition.op()) {
    // TODO(yzhao): Remove NIL, replace with has_cond() method to test the presence of condition.
    case Condition::NIL: {
      std::vector<std::string> code_lines = std::move(body);
      return code_lines;
    }
    case Condition::EQUAL: {
      if (condition.vars_size() != 2) {
        return error::InvalidArgument("Expect 2 variables, got $0", condition.vars_size());
      }
      std::vector<std::string> code_lines = {
          absl::Substitute("if ($0 == $1) {", condition.vars(0), condition.vars(1))};
      code_lines.insert(code_lines.end(), std::move_iterator(body.begin()),
                        std::move_iterator(body.end()));
      code_lines.push_back("}");
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
  PX_ASSIGN_OR_RETURN(std::vector<std::string> str_vec, expr); \
  MoveBackStrVec(std::move(str_vec), dst);

// TODO(yzhao): Wrap map stash action inside "{}" to avoid variable naming conflict.
//
// TODO(yzhao): Alternatively, leave map key as another Variable message (would be pre-generated
// as part of the physical IR).
StatusOr<std::vector<std::string>> GenMapStashAction(const MapStashAction& action) {
  std::vector<std::string> update_code_lines = {
      absl::Substitute("$0.update(&$1, &$2);", action.map_name(), action.key_variable_name(),
                       action.value_variable_name())};
  return GenCondition(action.cond(), std::move(update_code_lines));
}

std::string GenMapDeleteAction(const MapDeleteAction& action) {
  return absl::Substitute("$0.delete(&$1);", action.map_name(), action.key_variable_name());
}

std::string GenPerfBufferOutput(const PerfBufferOutput& output) {
  return absl::Substitute("BPF_PERF_OUTPUT($0);", output.name());
}

namespace {

StatusOr<std::vector<std::string>> GenPerfBufferOutputAction(
    const ir::physical::Struct& output_struct, const PerfBufferOutputAction& action) {
  std::string output_var_name = absl::StrCat(action.perf_buffer_name(), "_value");

  std::vector<std::string> code_lines;

  // Generate a temporary variable in a BPF map, to avoid crossing the BPF stack size limit.
  std::string arr_idx_var_name = absl::StrCat(output_var_name, "_idx");
  code_lines.push_back(absl::Substitute("uint32_t $0 = 0;", arr_idx_var_name));
  code_lines.push_back(absl::Substitute("struct $0* $1 = $2.lookup(&$3);",
                                        action.output_struct_name(), output_var_name,
                                        action.data_buffer_array_name(), arr_idx_var_name));
  code_lines.push_back(absl::Substitute("if ($0 == NULL) { return 0; }", output_var_name));

  int struct_field_index = 0;
  for (const auto& f : action.variable_names()) {
    code_lines.push_back(absl::Substitute("$0->$1 = $2;", output_var_name,
                                          output_struct.fields(struct_field_index++).name(), f));
  }

  code_lines.push_back(absl::Substitute("$0.perf_submit(ctx, $1, sizeof(*$1));",
                                        action.perf_buffer_name(), output_var_name));

  return code_lines;
}

ScalarType GetScalarVariableType(const Variable& var) {
  if (var.var_oneof_case() == Variable::VarOneofCase::kScalarVar) {
    return var.scalar_var().type();
  }

  LOG(DFATAL) << "Variable type must be ScalarType";
  return ScalarType::UNKNOWN;
}

StatusOr<std::string> GenScalarVarPrintk(
    const absl::flat_hash_map<std::string_view, const Variable*>& vars, const Printk& printk) {
  auto iter = vars.find(printk.scalar());

  if (iter == vars.end()) {
    return error::InvalidArgument("Variable '$0' is not defined", printk.scalar());
  }

  ScalarType type = GetScalarVariableType(*iter->second);

  PX_ASSIGN_OR_RETURN(std::string_view format_code, GetPrintFormatCode(type));

  return absl::Substitute(R"(bpf_trace_printk("$0: %$1\n", $0);)", printk.scalar(), format_code);
}

StatusOr<std::string> GenPrintk(const absl::flat_hash_map<std::string_view, const Variable*>& vars,
                                const Printk& printk) {
  switch (printk.content_oneof_case()) {
    case Printk::ContentOneofCase::kText:
      return absl::Substitute(R"(bpf_trace_printk("$0\n");)", printk.text());
    case Printk::ContentOneofCase::kScalar:
      return GenScalarVarPrintk(vars, printk);
    case Printk::ContentOneofCase::CONTENT_ONEOF_NOT_SET:
      PB_ENUM_SENTINEL_SWITCH_CLAUSE;
  }
  GCC_SWITCH_RETURN;
}

std::string GenMapVariable(const ir::physical::MapVariable& map_var) {
  return absl::Substitute("struct $0* $1 = $2.lookup(&$3);", map_var.type(), map_var.name(),
                          map_var.map_name(), map_var.key_variable_name());
}

Status CheckVarExists(const absl::flat_hash_map<std::string_view, const Variable*>& var_names,
                      std::string_view var_name, std::string_view context) {
  if (!var_names.contains(var_name)) {
    return error::InvalidArgument("Variable name '$0' was not defined [context = $1]", var_name,
                                  context);
  }
  return Status::OK();
}

bool IsVariableDefinition(const Variable& var) {
  if (var.has_scalar_var() && var.scalar_var().has_memory() &&
      var.scalar_var().memory().op() == ir::physical::ASSIGN_ONLY) {
    return false;
  }
  if (var.has_struct_var() && var.struct_var().op() == ir::physical::ASSIGN_ONLY) {
    return false;
  }
  return true;
}

}  // namespace

Status BCCCodeGenerator::GenVariable(
    const Variable& var, const absl::flat_hash_map<std::string_view, const Variable*>& vars,
    std::vector<std::string>* code_lines) const {
  switch (var.var_oneof_case()) {
    case Variable::VarOneofCase::kScalarVar: {
      MOVE_BACK_STR_VEC(GenScalarVariable(var.scalar_var()), code_lines);
      break;
    }
    case Variable::VarOneofCase::kMapVar: {
      code_lines->push_back(GenMapVariable(var.map_var()));
      break;
    }
    case Variable::VarOneofCase::kPtrLenVar: {
      MOVE_BACK_STR_VEC(GenPtrLenVariable(var.ptr_len_var()), code_lines);
      break;
    }
    case Variable::VarOneofCase::kStructVar: {
      const auto& st_var = var.struct_var();

      if (IsVariableDefinition(var)) {
        auto iter = structs_.find(st_var.type());
        if (iter == structs_.end()) {
          return error::InvalidArgument("Struct '$0' referenced in variable '$1' was not defined",
                                        st_var.type(), st_var.name());
        }
        if (iter->second->name() != st_var.type()) {
          return error::InvalidArgument("Names of the struct do not match, $0 vs. $1",
                                        iter->second->name(), st_var.type());
        }
      }

      for (const auto& fa : st_var.field_assignments()) {
        if (fa.value_oneof_case() == StructVariable::FieldAssignment::kVariableName) {
          PX_RETURN_IF_ERROR(
              CheckVarExists(vars, fa.variable_name(),
                             absl::Substitute("StructVariable '$0' field assignment '$1'",
                                              st_var.name(), fa.ShortDebugString())));
          // TODO(yzhao): Check variable types as well.
        }
      }

      MOVE_BACK_STR_VEC(GenStructVariable(st_var), code_lines);
      break;
    }
    case Variable::VarOneofCase::VAR_ONEOF_NOT_SET:
      return error::InvalidArgument("Variable is not set");
  }
  return Status::OK();
}

namespace {

std::string_view GetVariableName(const Variable& var) {
  switch (var.var_oneof_case()) {
    case Variable::VarOneofCase::kScalarVar:
      return var.scalar_var().name();
    case Variable::VarOneofCase::kMapVar:
      return var.map_var().name();
    case Variable::VarOneofCase::kStructVar:
      return var.struct_var().name();
    case Variable::VarOneofCase::kPtrLenVar:
      return var.ptr_len_var().name();
    case Variable::VarOneofCase::VAR_ONEOF_NOT_SET:
      LOG(DFATAL) << "Variable is not set";
      return {};
  }
  GCC_SWITCH_RETURN;
}

StatusOr<std::vector<std::string>> BCCCodeGenerator::GenerateConditionalBlock(
    const ir::physical::ConditionalBlock& cond_block) const {
  std::vector<std::string> code_lines;

  absl::flat_hash_map<std::string_view, const Variable*> vars;

  // NOTE: We do not check if these vars are defined prior. This is a lazy workaround of the fact
  // that the vars could be variable names or literal values. "NULL" is a case where the var being
  // a literal value.
  //
  // TODO(yzhao): Consider refining it to be more structured.

  for (const auto& var : cond_block.vars()) {
    std::string_view var_name = GetVariableName(var);

    if (IsVariableDefinition(var) && vars.contains(var_name)) {
      return error::InvalidArgument("Variable '$0' in ConditionalBlock '$1' was already defined",
                                    var.ShortDebugString(), cond_block.ShortDebugString());
    }

    vars[var_name] = &var;

    PX_RETURN_IF_ERROR(GenVariable(var, vars, &code_lines));
  }

  if (!cond_block.return_value().empty()) {
    code_lines.push_back(absl::Substitute("return $0;", cond_block.return_value()));
  }

  return GenCondition(cond_block.cond(), std::move(code_lines));
}

}  // namespace

StatusOr<std::vector<std::string>> BCCCodeGenerator::GenerateProbe(const Probe& probe) const {
  if (probe.name().empty()) {
    return error::InvalidArgument("Probe's name cannot be empty");
  }

  std::vector<std::string> code_lines;

  code_lines.push_back(absl::Substitute("int $0(struct pt_regs* ctx) {", probe.name()));

  absl::flat_hash_map<std::string_view, const Variable*> vars;

  for (const auto& var : probe.vars()) {
    std::string_view var_name = GetVariableName(var);

    if (IsVariableDefinition(var) && vars.contains(var_name)) {
      return error::InvalidArgument("Variable '$0' in Probe '$1' was already defined",
                                    var.ShortDebugString(), probe.name());
    }

    if (var.has_struct_var() && var.struct_var().is_output()) {
      // Leave output variable to be generated right before the output action.
      continue;
    }

    if (IsVariableDefinition(var)) {
      // Only index defined variable.
      vars[var_name] = &var;
    }

    PX_RETURN_IF_ERROR(GenVariable(var, vars, &code_lines));
  }

  for (const auto& block : probe.cond_blocks()) {
    PX_ASSIGN_OR_RETURN(std::vector<std::string> cond_code_lines, GenerateConditionalBlock(block));

    code_lines.insert(code_lines.end(), std::move_iterator(cond_code_lines.begin()),
                      std::move_iterator(cond_code_lines.end()));
  }

  for (const auto& action : probe.map_stash_actions()) {
    PX_RETURN_IF_ERROR(CheckVarExists(vars, action.key_variable_name(),
                                      absl::Substitute("BPF map '$0' key", action.map_name())));
    PX_RETURN_IF_ERROR(CheckVarExists(vars, action.value_variable_name(),
                                      absl::Substitute("BPF map '$0' value", action.map_name())));
    MOVE_BACK_STR_VEC(GenMapStashAction(action), &code_lines);
  }

  for (const auto& action : probe.map_delete_actions()) {
    PX_RETURN_IF_ERROR(CheckVarExists(vars, action.key_variable_name(),
                                      absl::Substitute("BPF map '$0' key", action.map_name())));
    code_lines.push_back(GenMapDeleteAction(action));
  }

  // Generate assignments to output variables right before the output action, and after all other
  // variables, such that the values got changed in the previous ConditionalBlock can take effect.
  //
  // TODO(yzhao): Consider letting PerfBufferOutput to include the output variable itself.
  for (const auto& var : probe.vars()) {
    // Skip variables that are not to be output.
    if (!var.has_struct_var() || !var.struct_var().is_output()) {
      continue;
    }

    std::string_view var_name = GetVariableName(var);

    if (IsVariableDefinition(var) && vars.contains(var_name)) {
      return error::InvalidArgument("Output variable '$0' in Probe '$1' was already defined.",
                                    var.ShortDebugString(), probe.name());
    }

    vars[var_name] = &var;

    PX_RETURN_IF_ERROR(GenVariable(var, vars, &code_lines));
  }

  for (const auto& action : probe.output_actions()) {
    auto iter = structs_.find(action.output_struct_name());
    if (iter == structs_.end()) {
      return error::InvalidArgument("Output struct '$0' is undefined", action.output_struct_name());
    }
    MOVE_BACK_STR_VEC(GenPerfBufferOutputAction(*iter->second, action), &code_lines);
  }

  for (const auto& printk : probe.printks()) {
    PX_ASSIGN_OR_RETURN(std::string code_line, GenPrintk(vars, printk));
    code_lines.push_back(std::move(code_line));
  }

  code_lines.push_back("return 0;");
  code_lines.push_back("}");

  return code_lines;
}

namespace {

StatusOr<std::vector<std::string>> GenMap(const Map& map) {
  PX_ASSIGN_OR_RETURN(std::string key_code, GenVariableType(map.key_type()));
  PX_ASSIGN_OR_RETURN(std::string value_code, GenVariableType(map.value_type()));
  std::vector<std::string> code_lines = {
      absl::Substitute("BPF_HASH($0, $1, $2);", map.name(), key_code, value_code)};
  return code_lines;
}

StatusOr<std::vector<std::string>> GenArray(const PerCPUArray& array) {
  if (array.capacity() <= 0) {
    return error::InvalidArgument("Input array capacity cannot be less than 1, got: $0",
                                  array.capacity());
  }
  PX_ASSIGN_OR_RETURN(std::string key_code, GenVariableType(array.type()));
  std::vector<std::string> code_lines = {
      absl::Substitute("BPF_PERCPU_ARRAY($0, $1, $2);", array.name(), key_code, array.capacity())};
  return code_lines;
}

std::vector<std::string> GenIncludes() {
  return {
      // For struct task_struct.
      "#include <linux/sched.h>",
  };
}

std::vector<std::string> GenMacros() {
  return {"#define __inline inline __attribute__((__always_inline__))"};
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

std::vector<std::string> GenBlobType(std::string_view type_name, int size,
                                     bool include_decoder_index = false) {
  // Size must be a power of 2.
  DCHECK_EQ(size & (size - 1), 0);

  std::vector<std::string> code_lines = {
      // Length field must be the first field, which is used to determine how much bytes to read
      // during decoding.
      absl::Substitute("$0 {", type_name), "  uint64_t len;"};

  // This by default accounts for the length and truncated bytes.
  int overhead_size = sizeof(uint64_t) + sizeof(uint8_t);

  if (include_decoder_index) {
    // TODO(yzhao): Change to use uint16_t.
    code_lines.push_back("  int8_t decoder_idx;");
    overhead_size += sizeof(int8_t);
  }

  code_lines.insert(code_lines.end(),
                    {
                        absl::Substitute("  uint8_t buf[$0-$1];", size, overhead_size),
                        "  uint8_t truncated;",
                        "};",
                    });

  return code_lines;
}

// Returns the type definitions of pre-defined data structures.
std::vector<std::string> GenTypes() {
  std::vector<std::string> code_lines;
  // Create underlying blob types for strings, byte arrays, etc.
  for (auto& size : std::set{kStructStringSize, kStructByteArraySize}) {
    const std::string type_name = absl::StrCat("struct blob", size);
    MoveBackStrVec(GenBlobType(type_name, size), &code_lines);
  }
  MoveBackStrVec(GenBlobType(kStructBlob, kStructBlobSize, /*include_decoder_index*/ true),
                 &code_lines);
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

  for (const auto& array : program_.arrays()) {
    MOVE_BACK_STR_VEC(GenArray(array), &code_lines);
  }

  if (program_.language() == ir::shared::Language::GOLANG) {
    // goid() accesses BPF map.
    MoveBackStrVec(GenGOID(), &code_lines);
  }

  for (const auto& output : program_.outputs()) {
    code_lines.push_back(GenPerfBufferOutput(output));
  }

  for (const auto& probe : program_.probes()) {
    MOVE_BACK_STR_VEC(GenerateProbe(probe), &code_lines);
  }

  return code_lines;
}

}  // namespace

StatusOr<std::string> GenBCCProgram(const Program& program) {
  BCCCodeGenerator generator(program);
  PX_ASSIGN_OR_RETURN(std::vector<std::string> code_lines, generator.GenerateCodeLines());
  return absl::StrJoin(code_lines, "\n");
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
