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

#include "src/carnot/planner/objects/pixie_module.h"

#include <memory>
#include <vector>

#include "src/carnot/planner/ir/time.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/exporter.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/otel.h"
#include "src/carnot/planner/objects/plugin.h"
#include "src/carnot/planner/objects/time.h"
#include "src/carnot/planner/objects/viz_object.h"
#include "src/common/base/statusor.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<PixieModule>> PixieModule::Create(
    IR* graph, CompilerState* compiler_state, ASTVisitor* ast_visitor, bool func_based_exec,
    const absl::flat_hash_set<std::string>& reserved_names) {
  auto pixie_module = std::shared_ptr<PixieModule>(
      new PixieModule(graph, compiler_state, ast_visitor, func_based_exec, reserved_names));

  PX_RETURN_IF_ERROR(pixie_module->Init());
  return pixie_module;
}

StatusOr<QLObjectPtr> UDFHandler(IR* graph, std::string name, const pypa::AstPtr& ast,
                                 const ParsedArgs& args, ASTVisitor* visitor) {
  std::vector<ExpressionIR*> expr_args;
  for (const auto& arg : args.variable_args()) {
    if (!ExprObject::IsExprObject(arg)) {
      return arg->CreateError("Argument to '$0' must be an expression, received $1", name,
                              arg->name());
    }
    expr_args.push_back(static_cast<ExprObject*>(arg.get())->expr());
  }
  FuncIR::Op op{FuncIR::Opcode::non_op, "", name};
  PX_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, expr_args));
  return ExprObject::Create(node, visitor);
}

Status PixieModule::RegisterUDFFuncs() {
  auto func_names = compiler_state_->registry_info()->func_names();
  for (const auto& name : func_names) {
    PX_ASSIGN_OR_RETURN(
        std::shared_ptr<FuncObject> fn_obj,
        FuncObject::Create(name, {}, {},
                           /* has_variable_len_args */ true,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&UDFHandler, graph_, std::string(name), std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3),
                           ast_visitor()));

    AddMethod(std::string(name), fn_obj);
  }
  return Status::OK();
}

StatusOr<std::string> PrepareDefaultUDTFArg(const planpb::ScalarValue& scalar_value) {
  switch (scalar_value.data_type()) {
    case types::BOOLEAN: {
      if (scalar_value.bool_value()) {
        return std::string("True");
      } else {
        return std::string("False");
      }
    }
    case types::INT64: {
      return absl::Substitute("$0", scalar_value.int64_value());
    }
    case types::FLOAT64: {
      return absl::Substitute("$0", scalar_value.float64_value());
    }
    case types::STRING: {
      return scalar_value.string_value();
    }
    case types::UINT128: {
      std::string upid_as_str =
          sole::rebuild(scalar_value.uint128_value().high(), scalar_value.uint128_value().low())
              .str();
      return absl::Substitute("$0.$1('$2')", PixieModule::kPixieModuleObjName,
                              PixieModule::kUInt128ConversionID, upid_as_str);
    }
    default: {
      return error::InvalidArgument("$0 not handled as a default value",
                                    types::ToString(scalar_value.data_type()));
    }
  }
}

StatusOr<ExpressionIR*> EvaluateUDTFArg(IR* graph, IRNode* arg_node,
                                        const udfspb::UDTFSourceSpec::Arg& arg) {
  // Processd for the data node instead.
  if (!Match(arg_node, DataNode())) {
    return arg_node->CreateIRNodeError("Expected '$0' to be of type $1, received a $2", arg.name(),
                                       arg.arg_type(), arg_node->type_string());
  }
  DataIR* data_node = static_cast<DataIR*>(arg_node);
  switch (arg.semantic_type()) {
    case types::ST_NONE: {
      if (data_node->EvaluatedDataType() != arg.arg_type()) {
        return arg_node->CreateIRNodeError("Expected '$0' to be a $1, received a $2", arg.name(),
                                           types::ToString(arg.arg_type()),
                                           types::ToString(data_node->EvaluatedDataType()));
      }
      return data_node;
    }
    case types::ST_UPID: {
      DCHECK_EQ(arg.arg_type(), types::UINT128);
      if (!Match(data_node, UInt128Value()) && !Match(data_node, String())) {
        return arg_node->CreateIRNodeError(
            "UPID must be a uint128 or str that converts to UUID, received a $0",
            types::ToString(arg.arg_type()));
      }
      if (Match(data_node, String())) {
        // If the parse fails, then we don't have a properly formatted upid.
        return graph->CreateNode<UInt128IR>(arg_node->ast(),
                                            static_cast<StringIR*>(data_node)->str());
      }
      // Otherwise match uint128 and do nothing.
      return data_node;
    }
    case types::ST_AGENT_UID: {
      DCHECK_EQ(arg.arg_type(), types::STRING);
      if (!Match(data_node, String())) {
        return arg_node->CreateIRNodeError("Agent UID must be a string.");
      }
      return data_node;
    }
    case types::ST_UNSPECIFIED:
    default:
      return arg_node->CreateIRNodeError(
          "error in arg definition of UDTF: semantic type must at least be specified as ST_NONE");
  }
}

StatusOr<QLObjectPtr> UDTFSourceHandler(CompilerState* compiler_state, IR* graph,
                                        const udfspb::UDTFSourceSpec& udtf_source_spec,
                                        const pypa::AstPtr& ast, const ParsedArgs& args,
                                        ASTVisitor* visitor) {
  absl::flat_hash_map<std::string, ExpressionIR*> arg_map;
  for (const auto& arg : udtf_source_spec.args()) {
    DCHECK(args.args().contains(arg.name()));
    PX_ASSIGN_OR_RETURN(IRNode * arg_node, GetArgAs<IRNode>(ast, args, arg.name()));
    PX_ASSIGN_OR_RETURN(arg_map[arg.name()], EvaluateUDTFArg(graph, arg_node, arg));
  }
  PX_ASSIGN_OR_RETURN(
      UDTFSourceIR * udtf_source,
      graph->CreateNode<UDTFSourceIR>(ast, udtf_source_spec.name(), arg_map, udtf_source_spec));
  return Dataframe::Create(compiler_state, udtf_source, visitor);
}

Status PixieModule::RegisterUDTFs() {
  for (const auto& udtf : compiler_state_->registry_info()->udtfs()) {
    std::vector<std::string> argument_names;
    absl::flat_hash_map<std::string, std::string> default_values;
    for (const auto& arg : udtf.args()) {
      argument_names.push_back(arg.name());
      if (arg.has_default_value()) {
        DCHECK_EQ(arg.default_value().data_type(), arg.arg_type());
        PX_ASSIGN_OR_RETURN(default_values[arg.name()], PrepareDefaultUDTFArg(arg.default_value()));
      }
    }

    PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> fn_obj,
                        FuncObject::Create(udtf.name(), argument_names, default_values,
                                           /* has_variable_len_args */ false,
                                           /* has_variable_len_kwargs */ false,
                                           std::bind(&UDTFSourceHandler, compiler_state_, graph_,
                                                     udtf, std::placeholders::_1,
                                                     std::placeholders::_2, std::placeholders::_3),
                                           ast_visitor()));

    AddMethod(udtf.name(), fn_obj);
  }
  return Status::OK();
}

StatusOr<QLObjectPtr> NowEval(CompilerState* compiler_state, IR* graph, const pypa::AstPtr& ast,
                              const ParsedArgs&, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(IntIR * time_now,
                      graph->CreateNode<IntIR>(ast, compiler_state->time_now().val));
  return ExprObject::Create(time_now, visitor);
}

StatusOr<QLObjectPtr> TimeEval(IR* graph, std::chrono::nanoseconds scale_ns,
                               const pypa::AstPtr& ast, const ParsedArgs& args,
                               ASTVisitor* visitor) {
  std::vector<ExpressionIR*> expr_args;

  PX_ASSIGN_OR_RETURN(IntIR * unit, GetArgAs<IntIR>(ast, args, "unit"));
  // TODO(philkuz) cast as durationnanos.
  PX_ASSIGN_OR_RETURN(IntIR * duration_nanos,
                      graph->CreateNode<IntIR>(ast, unit->val() * scale_ns.count()));
  return ExprObject::Create(duration_nanos, visitor);
}

StatusOr<QLObjectPtr> UInt128Conversion(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                        ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StringIR * uuid_str, GetArgAs<StringIR>(ast, args, "uuid"));
  auto upid_or_s = md::UPID::ParseFromUUIDString(static_cast<StringIR*>(uuid_str)->str());
  if (!upid_or_s.ok()) {
    return uuid_str->CreateIRNodeError(upid_or_s.msg());
  }
  PX_ASSIGN_OR_RETURN(UInt128IR * uint128_ir,
                      graph->CreateNode<UInt128IR>(ast, upid_or_s.ConsumeValueOrDie().value()));

  return ExprObject::Create(uint128_ir, visitor);
}

StatusOr<QLObjectPtr> UPIDConstructor(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                      ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(IntIR * asid_ir, GetArgAs<IntIR>(ast, args, "asid"));
  PX_ASSIGN_OR_RETURN(IntIR * pid_ir, GetArgAs<IntIR>(ast, args, "pid"));
  PX_ASSIGN_OR_RETURN(IntIR * ts_ns_ir, GetArgAs<IntIR>(ast, args, "ts_ns"));
  // Check to make sure asid and pid values are within range of the uint32 values.

  if (asid_ir->val() > UINT32_MAX || asid_ir->val() < 0) {
    return asid_ir->CreateIRNodeError(
        "asid value '$0' out of range for a 32-bit number (min is 0 and max is $1)", asid_ir->val(),
        UINT32_MAX);
  }

  if (pid_ir->val() > UINT32_MAX || pid_ir->val() < 0) {
    return pid_ir->CreateIRNodeError(
        "pid value '$0' out of range for a 32-bit number (min is 0 and max is $1)", pid_ir->val(),
        UINT32_MAX);
  }

  auto upid = md::UPID(asid_ir->val(), pid_ir->val(), ts_ns_ir->val());
  PX_ASSIGN_OR_RETURN(UInt128IR * uint128_ir, graph->CreateNode<UInt128IR>(ast, upid.value()));
  uint128_ir->SetTypeCast(ValueType::Create(uint128_ir->EvaluatedDataType(), types::ST_UPID));

  return ExprObject::Create(uint128_ir, visitor);
}

StatusOr<QLObjectPtr> AbsTime(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                              ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StringIR * date_str_ir, GetArgAs<StringIR>(ast, args, "date_string"));
  PX_ASSIGN_OR_RETURN(StringIR * format_str_ir, GetArgAs<StringIR>(ast, args, "format"));
  PX_ASSIGN_OR_RETURN(int64_t time_ns, ParseAbsFmt(date_str_ir, format_str_ir->str()));
  PX_ASSIGN_OR_RETURN(IntIR * time_count, graph->CreateNode<IntIR>(ast, time_ns));
  return StatusOr<QLObjectPtr>(ExprObject::Create(time_count, visitor));
}

StatusOr<QLObjectPtr> EqualsAny(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(ExpressionIR * value_ir, GetArgAs<ExpressionIR>(ast, args, "value"));
  auto comparisons = args.GetArg("comparisons");
  if (!CollectionObject::IsCollection(comparisons)) {
    return comparisons->CreateError("'comparisons' must be a collection");
  }
  auto comparison_values = std::static_pointer_cast<CollectionObject>(comparisons);
  if (comparison_values->items().empty()) {
    return comparisons->CreateError("'comparisons' cannot be an empty collection");
  }

  ExpressionIR* or_expr = nullptr;

  for (const auto& [idx, value] : Enumerate(comparison_values->items())) {
    if (!ExprObject::IsExprObject(value)) {
      return value->CreateError("Expected item in index $0 be an expression, received $1", idx,
                                value->name());
    }
    auto comparison_expr = static_cast<ExprObject*>(value.get())->expr();
    FuncIR::Op equal{FuncIR::eq, "equal", "equal"};
    PX_ASSIGN_OR_RETURN(auto new_equals, graph->CreateNode<FuncIR>(comparison_expr->ast(), equal,
                                                                   std::vector<ExpressionIR*>{
                                                                       value_ir, comparison_expr}));
    if (or_expr == nullptr) {
      or_expr = new_equals;
      continue;
    }
    FuncIR::Op logicalOr{FuncIR::logor, "logicalOr", "logicalOr"};
    PX_ASSIGN_OR_RETURN(or_expr,
                        graph->CreateNode<FuncIR>(comparison_expr->ast(), logicalOr,
                                                  std::vector<ExpressionIR*>{or_expr, new_equals}));
  }
  DCHECK(or_expr);
  return StatusOr<QLObjectPtr>(ExprObject::Create(or_expr, visitor));
}

// px.script_reference is parsed and converted to a call to the private method px._script_reference.
// This is so that we can support a cleaner, dictionary-based representation for script args, which
// is not currently supported in Carnot.
StatusOr<QLObjectPtr> ScriptReference(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                      ASTVisitor* visitor) {
  std::vector<ExpressionIR*> udf_args;

  PX_ASSIGN_OR_RETURN(ExpressionIR * label_ir, GetArgAs<ExpressionIR>(ast, args, "label"));
  if (!Match(label_ir, Func()) && !Match(label_ir, String()) && !Match(label_ir, ColumnNode())) {
    return label_ir->CreateIRNodeError(
        "Expected first argument 'label' of function 'script_reference' to be of type string, "
        "column, or function, received $0",
        label_ir->type_string());
  }

  PX_ASSIGN_OR_RETURN(ExpressionIR * script_ir, GetArgAs<ExpressionIR>(ast, args, "script"));
  if (!Match(script_ir, Func()) && !Match(script_ir, String()) && !Match(script_ir, ColumnNode())) {
    return script_ir->CreateIRNodeError(
        "Expected first argument 'script' of function 'script_reference' to be of type string, "
        "column, or function, received $0",
        script_ir->type_string());
  }

  udf_args.push_back(label_ir);
  udf_args.push_back(script_ir);

  QLObjectPtr script_args = args.GetArg("args");
  if (!DictObject::IsDict(script_args)) {
    return script_args->CreateError(
        "Expected third argument 'args' of function 'script_reference' to be a dictionary, "
        "received "
        "$0",
        script_args->name());
  }
  auto dict = static_cast<DictObject*>(script_args.get());
  auto values = dict->values();
  auto keys = dict->keys();
  CHECK_EQ(values.size(), keys.size());

  for (const auto& [idx, key] : Enumerate(keys)) {
    PX_ASSIGN_OR_RETURN(StringIR * key_str_ir, GetArgAs<StringIR>(ast, key, "key"));
    PX_ASSIGN_OR_RETURN(ExpressionIR * val_ir, GetArgAs<ExpressionIR>(ast, values[idx], "label"));

    if (!Match(val_ir, Func()) && !Match(val_ir, String()) && !Match(val_ir, ColumnNode())) {
      return val_ir->CreateIRNodeError(
          "Expected dictionary values to be of type string, column, or function in third argument "
          "'args' to function 'script_reference', received $0",
          val_ir->type_string());
    }

    udf_args.push_back(key_str_ir);
    udf_args.push_back(val_ir);
  }

  FuncIR::Op op{FuncIR::Opcode::non_op, "", "_script_reference"};
  PX_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, udf_args));
  return ExprObject::Create(node, visitor);
}

StatusOr<QLObjectPtr> ParseDuration(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StringIR * duration_string, GetArgAs<StringIR>(ast, args, "duration"));
  auto int_or_s = StringToTimeInt(duration_string->str());
  if (!int_or_s.ok()) {
    return WrapAstError(duration_string->ast(), int_or_s.status());
  }

  PX_ASSIGN_OR_RETURN(IntIR * node, graph->CreateNode<IntIR>(ast, int_or_s.ConsumeValueOrDie()));
  return ExprObject::Create(node, visitor);
}

StatusOr<QLObjectPtr> FormatDuration(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args,
                                     ASTVisitor* visitor) {
  std::string s;
  PX_ASSIGN_OR_RETURN(IntIR * duration, GetArgAs<IntIR>(ast, args, "duration"));
  bool negative = duration->val() < 0;
  auto duration_uint = negative ? -1 * duration->val() : duration->val();

  int64_t ms_nanos = 1000 * 1000;
  int64_t s_nanos = 1000 * ms_nanos;
  int64_t min_nanos = 60 * s_nanos;
  int64_t hour_nanos = 60 * min_nanos;
  int64_t day_nanos = 24 * hour_nanos;
  if (duration_uint >= day_nanos) {
    s = absl::Substitute("$0d", duration_uint / day_nanos);
  } else if (duration_uint >= hour_nanos) {
    s = absl::Substitute("$0h", duration_uint / hour_nanos);
  } else if (duration_uint >= min_nanos) {
    s = absl::Substitute("$0m", duration_uint / min_nanos);
  } else if (duration_uint >= s_nanos) {
    s = absl::Substitute("$0s", duration_uint / s_nanos);
  } else {
    s = absl::Substitute("$0ms", duration_uint / ms_nanos);
  }

  if (negative) {
    s = "-" + s;
  }
  auto node = graph->CreateNode<StringIR>(ast, s).ConsumeValueOrDie();
  return ExprObject::Create(node, visitor);
}

StatusOr<QLObjectPtr> ParseTime(int64_t time_now, IR* graph, const pypa::AstPtr& ast,
                                const ParsedArgs& args, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(ExpressionIR * time_ir, GetArgAs<ExpressionIR>(ast, args, "time"));

  auto int_or_s = ParseAllTimeFormats(time_now, time_ir);
  if (!int_or_s.ok()) {
    return WrapAstError(time_ir->ast(), int_or_s.status());
  }

  PX_ASSIGN_OR_RETURN(TimeIR * node, graph->CreateNode<TimeIR>(ast, int_or_s.ConsumeValueOrDie()));
  return ExprObject::Create(node, visitor);
}

StatusOr<QLObjectPtr> Export(const pypa::AstPtr& ast, const ParsedArgs& args, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto df, GetAsDataFrame(args.GetArg("out")));

  QLObjectPtr spec = args.GetArg("export_spec");
  if (!Exporter::IsExporter(spec)) {
    return spec->CreateError("Expected 'spec' to be an export config. Received a $0", spec->name());
  }

  auto exporter = std::static_pointer_cast<Exporter>(spec);
  PX_RETURN_IF_ERROR(exporter->Export(ast, df.get()));

  return StatusOr(std::make_shared<NoneObject>(visitor));
}

Status PixieModule::RegisterCompileTimeFuncs() {
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> now_fn,
      FuncObject::Create(kNowOpID, {}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&NowEval, compiler_state_, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(now_fn->SetDocString(kNowOpDocstring));
  AddMethod(kNowOpID, now_fn);

  for (const auto& [time_fn_id, time_scale_ns] : kTimeFuncValues) {
    PX_ASSIGN_OR_RETURN(
        std::shared_ptr<FuncObject> time_fn,
        FuncObject::Create(time_fn_id, {"unit"}, {},
                           /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                           std::bind(&TimeEval, graph_, time_scale_ns, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3),
                           ast_visitor()));
    PX_RETURN_IF_ERROR(time_fn->SetDocString(absl::Substitute(kTimeFuncDocstringTpl, time_fn_id)));
    AddMethod(time_fn_id, time_fn);
  }

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> uuid_str_fn,
      FuncObject::Create(kUInt128ConversionID, {"uuid"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&UInt128Conversion, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(uuid_str_fn->SetDocString(kUInt128ConversionDocstring));
  AddMethod(kUInt128ConversionID, uuid_str_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> upid_constructor_fn,
      FuncObject::Create(kMakeUPIDID, {"asid", "pid", "ts_ns"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&UPIDConstructor, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PX_RETURN_IF_ERROR(upid_constructor_fn->SetDocString(kMakeUPIDDocstring));
  AddMethod(kMakeUPIDID, upid_constructor_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> abs_time_fn,
      FuncObject::Create(kAbsTimeOpID, {"date_string", "format"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&AbsTime, graph_, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(abs_time_fn->SetDocString(kAbsTimeDocstring));
  AddMethod(kAbsTimeOpID, abs_time_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> equals_any_fn,
      FuncObject::Create(kEqualsAnyID, {"value", "comparisons"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&EqualsAny, graph_, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(equals_any_fn->SetDocString(kEqualsAnyDocstring));
  AddMethod(kEqualsAnyID, equals_any_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> script_reference_fn,
      FuncObject::Create(kScriptReferenceID, {"label", "script", "args"}, {{"args", "{}"}},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&ScriptReference, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(script_reference_fn->SetDocString(kScriptReferenceDocstring));
  AddMethod(kScriptReferenceID, script_reference_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> parse_duration_fn,
      FuncObject::Create(kParseDurationOpID, {"duration"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&ParseDuration, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(parse_duration_fn->SetDocString(kParseDurationDocstring));
  AddMethod(kParseDurationOpID, parse_duration_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> parse_time_fn,
      FuncObject::Create(
          kParseTimeOpID, {"time"}, {},
          /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
          std::bind(&ParseTime, compiler_state_->time_now().val, graph_, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> format_duration_fn,
      FuncObject::Create(kFormatDurationOpID, {"duration"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&FormatDuration, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  AddMethod(kFormatDurationOpID, format_duration_fn);
  PX_RETURN_IF_ERROR(format_duration_fn->SetDocString(kFormatDurationDocstring));

  PX_RETURN_IF_ERROR(parse_time_fn->SetDocString(kParseTimeDocstring));
  AddMethod(kParseTimeOpID, parse_time_fn);
  return Status::OK();
}

Status PixieModule::RegisterTypeObjs() {
  PX_ASSIGN_OR_RETURN(auto time_type_object, TypeObject::Create(IRNodeType::kTime, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kTimeTypeName, time_type_object));

  // Service
  PX_ASSIGN_OR_RETURN(auto service_type_object,
                      TypeObject::Create(types::STRING, types::ST_SERVICE_NAME, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kServiceTypeName, service_type_object));

  // Pod
  PX_ASSIGN_OR_RETURN(auto pod_type_object,
                      TypeObject::Create(types::STRING, types::ST_POD_NAME, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kPodTypeName, pod_type_object));

  // Node
  PX_ASSIGN_OR_RETURN(auto node_type_object,
                      TypeObject::Create(types::STRING, types::ST_NODE_NAME, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kNodeTypeName, node_type_object));

  // Namespace
  PX_ASSIGN_OR_RETURN(auto namespace_type_object,
                      TypeObject::Create(types::STRING, types::ST_NAMESPACE_NAME, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kNamespaceTypeName, namespace_type_object));

  // Container
  PX_ASSIGN_OR_RETURN(auto container_type_object,
                      TypeObject::Create(types::STRING, types::ST_CONTAINER_NAME, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kContainerTypeName, container_type_object));

  // UPID
  PX_ASSIGN_OR_RETURN(auto upid_type_object,
                      TypeObject::Create(types::UINT128, types::ST_UPID, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kUPIDTypeName, upid_type_object));

  // Bytes
  PX_ASSIGN_OR_RETURN(auto bytes_type_object,
                      TypeObject::Create(types::INT64, types::ST_BYTES, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kBytesTypeName, bytes_type_object));

  // Duration ns
  PX_ASSIGN_OR_RETURN(auto duration_type_object,
                      TypeObject::Create(types::INT64, types::ST_DURATION_NS, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kDurationNSTypeName, duration_type_object));

  // Percent
  PX_ASSIGN_OR_RETURN(auto percent_type_object,
                      TypeObject::Create(types::FLOAT64, types::ST_PERCENT, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(PixieModule::kPercentTypeName, percent_type_object));

  return Status::OK();
}

StatusOr<QLObjectPtr> NoopDisplayHandler(IR*, CompilerState*, const pypa::AstPtr&,
                                         const ParsedArgs&, ASTVisitor* visitor) {
  // TODO(PP-1773): Surface a warning to the user when calling px.display in a function based
  // execution regime. For now, we'll allow it and just have it do nothing.
  return StatusOr(std::make_shared<NoneObject>(visitor));
}

StatusOr<QLObjectPtr> DisplayHandler(IR* graph, CompilerState* compiler_state,
                                     const pypa::AstPtr& ast, const ParsedArgs& args,
                                     ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto df, GetAsDataFrame(args.GetArg("out")));
  PX_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(ast, args, "name"));

  PX_RETURN_IF_ERROR(AddResultSink(graph, ast, name->str(), df->op(),
                                   compiler_state->result_address(),
                                   compiler_state->result_ssl_targetname()));
  return StatusOr(std::make_shared<NoneObject>(visitor));
}

StatusOr<QLObjectPtr> DebugDisplayHandler(IR* graph, CompilerState* compiler_state,
                                          const absl::flat_hash_set<std::string>& reserved_names,
                                          const pypa::AstPtr& ast, const ParsedArgs& args,
                                          ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(auto df, GetAsDataFrame(args.GetArg("out")));
  PX_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(ast, args, "name"));

  std::string out_name = PixieModule::kDebugTablePrefix + name->str();
  std::string out_name_base = out_name;
  // Remove ambiguitiy if there is repeated out_name in the display call and func_based_exec.
  int64_t i = 1;
  while (reserved_names.contains(out_name)) {
    out_name = absl::Substitute("$0_$1", out_name_base, i);
    ++i;
  }

  PX_RETURN_IF_ERROR(AddResultSink(graph, ast, out_name, df->op(), compiler_state->result_address(),
                                   compiler_state->result_ssl_targetname()));
  return StatusOr(std::make_shared<NoneObject>(visitor));
}

Status PixieModule::Init() {
  PX_RETURN_IF_ERROR(RegisterUDFFuncs());
  PX_RETURN_IF_ERROR(RegisterCompileTimeFuncs());
  PX_RETURN_IF_ERROR(RegisterUDTFs());
  PX_RETURN_IF_ERROR(RegisterTypeObjs());

  auto display_handler = func_based_exec_ ? &NoopDisplayHandler : &DisplayHandler;
  // Setup methods.
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> display_fn,
      FuncObject::Create(kDisplayOpID, {"out", "name", "cols"},
                         {{"name", "'output'"}, {"cols", "[]"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(display_handler, graph_, compiler_state_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  PX_RETURN_IF_ERROR(display_fn->SetDocString(kDisplayOpDocstring));
  AddMethod(kDisplayOpID, display_fn);

  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> export_fn,
                      FuncObject::Create(kExportOpID, {"out", "export_spec"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&Export, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));

  PX_RETURN_IF_ERROR(export_fn->SetDocString(kExportOpDocstring));
  AddMethod(kExportOpID, export_fn);

  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> debug_fn,
      FuncObject::Create(
          kDebugOpID, {"out", "name", "cols"}, {{"name", "'output'"}, {"cols", "[]"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&DebugDisplayHandler, graph_, compiler_state_, reserved_names_,
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));

  PX_RETURN_IF_ERROR(debug_fn->SetDocString(kDebugOpDocstring));
  AddMethod(kDebugOpID, debug_fn);

  PX_ASSIGN_OR_RETURN(auto base_df, Dataframe::Create(compiler_state_, graph_, ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(kDataframeOpID, base_df));
  PX_ASSIGN_OR_RETURN(auto viz, VisualizationObject::Create(ast_visitor()));
  PX_RETURN_IF_ERROR(AssignAttribute(kVisAttrID, viz));

  PX_ASSIGN_OR_RETURN(auto otel, OTelModule::Create(compiler_state_, ast_visitor(), graph_));
  PX_RETURN_IF_ERROR(AssignAttribute("otel", otel));

  PX_ASSIGN_OR_RETURN(
      auto plugin, PluginModule::Create(compiler_state_->plugin_config(), ast_visitor(), graph_));
  PX_RETURN_IF_ERROR(AssignAttribute("plugin", plugin));
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
