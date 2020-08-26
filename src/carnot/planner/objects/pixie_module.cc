#include "src/carnot/planner/objects/pixie_module.h"

#include <vector>

#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/viz_object.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/types/magic_enum.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
constexpr const char* const PixieModule::kTimeFuncs[];

StatusOr<std::shared_ptr<PixieModule>> PixieModule::Create(
    IR* graph, CompilerState* compiler_state, ASTVisitor* ast_visitor, bool func_based_exec,
    const absl::flat_hash_set<std::string>& reserved_names) {
  auto pixie_module = std::shared_ptr<PixieModule>(
      new PixieModule(graph, compiler_state, ast_visitor, func_based_exec, reserved_names));

  PL_RETURN_IF_ERROR(pixie_module->Init());
  return pixie_module;
}

Status PixieModule::RegisterUDFFuncs() {
  // TODO(philkuz) (PL-1189) remove this when the udf names no longer have the 'pl.' prefix.
  auto func_names = compiler_state_->registry_info()->func_names();
  for (const auto& name : func_names) {
    // attributes_.emplace(stripped_name);

    PL_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> fn_obj,
                        FuncObject::Create(name, {}, {},
                                           /* has_variable_len_args */ true,
                                           /* has_variable_len_kwargs */ false,
                                           std::bind(&UDFHandler::Eval, graph_, std::string(name),
                                                     std::placeholders::_1, std::placeholders::_2,
                                                     std::placeholders::_3),
                                           ast_visitor()));

    AddMethod(std::string(name), fn_obj);
  }
  // TODO(philkuz) (PL-1189) enable this.
  // attributes_ = compiler_state_->registry_info()->func_names()
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
                              PixieModule::kUInt128ConversionId, upid_as_str);
    }
    default: {
      return error::InvalidArgument("$0 not handled as a default value",
                                    magic_enum::enum_name(scalar_value.data_type()));
    }
  }
}

Status PixieModule::RegisterUDTFs() {
  for (const auto& udtf : compiler_state_->registry_info()->udtfs()) {
    std::vector<std::string> argument_names;
    absl::flat_hash_map<std::string, std::string> default_values;
    for (const auto& arg : udtf.args()) {
      argument_names.push_back(arg.name());
      if (arg.has_default_value()) {
        DCHECK_EQ(arg.default_value().data_type(), arg.arg_type());
        PL_ASSIGN_OR_RETURN(default_values[arg.name()], PrepareDefaultUDTFArg(arg.default_value()));
      }
    }

    PL_ASSIGN_OR_RETURN(
        std::shared_ptr<FuncObject> fn_obj,
        FuncObject::Create(udtf.name(), argument_names, default_values,
                           /* has_variable_len_args */ false,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&UDTFSourceHandler::Eval, graph_, udtf, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3),
                           ast_visitor()));

    AddMethod(udtf.name(), fn_obj);
  }
  return Status::OK();
}

Status PixieModule::RegisterCompileTimeFuncs() {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> now_fn,
      FuncObject::Create(kNowOpID, {}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&CompileTimeFuncHandler::NowEval, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PL_RETURN_IF_ERROR(now_fn->SetDocString(kNowOpDocstring));
  AddMethod(kNowOpID, now_fn);
  for (const auto& time : kTimeFuncs) {
    PL_RETURN_IF_ERROR(RegisterCompileTimeUnitFunction(time));
  }

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> uuid_str_fn,
      FuncObject::Create(
          kUInt128ConversionId, {"uuid"}, {},
          /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
          std::bind(&CompileTimeFuncHandler::UInt128Conversion, graph_, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  PL_RETURN_IF_ERROR(uuid_str_fn->SetDocString(kUInt128ConversionDocstring));
  AddMethod(kUInt128ConversionId, uuid_str_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> upid_constructor_fn,
      FuncObject::Create(
          kMakeUPIDId, {"asid", "pid", "ts_ns"}, {},
          /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
          std::bind(&CompileTimeFuncHandler::UPIDConstructor, graph_, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  PL_RETURN_IF_ERROR(upid_constructor_fn->SetDocString(kMakeUPIDDocstring));
  AddMethod(kMakeUPIDId, upid_constructor_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> abs_time_fn,
      FuncObject::Create(kAbsTimeOpID, {"date_string", "format"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&CompileTimeFuncHandler::AbsTime, graph_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  PL_RETURN_IF_ERROR(abs_time_fn->SetDocString(kAbsTimeDocstring));
  AddMethod(kAbsTimeOpID, abs_time_fn);
  return Status::OK();
}

Status PixieModule::RegisterCompileTimeUnitFunction(std::string name) {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> time_fn,
      FuncObject::Create(
          name, {"unit"}, {},
          /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
          std::bind(&CompileTimeFuncHandler::TimeEval, graph_, name, std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  PL_RETURN_IF_ERROR(time_fn->SetDocString(absl::Substitute(kTimeFuncDocstringTpl, name)));
  AddMethod(name.data(), time_fn);
  return Status::OK();
}

Status PixieModule::RegisterTypeObjs() {
  PL_ASSIGN_OR_RETURN(auto time_type_object, TypeObject::Create(IRNodeType::kTime, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kTimeTypeName, time_type_object));

  // Service
  PL_ASSIGN_OR_RETURN(auto service_type_object,
                      TypeObject::Create(types::STRING, types::ST_SERVICE_NAME, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kServiceTypeName, service_type_object));

  // Pod
  PL_ASSIGN_OR_RETURN(auto pod_type_object,
                      TypeObject::Create(types::STRING, types::ST_POD_NAME, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kPodTypeName, pod_type_object));

  // Node
  PL_ASSIGN_OR_RETURN(auto node_type_object,
                      TypeObject::Create(types::STRING, types::ST_NODE_NAME, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kNodeTypeName, node_type_object));

  // Namespace
  PL_ASSIGN_OR_RETURN(auto namespace_type_object,
                      TypeObject::Create(types::STRING, types::ST_NAMESPACE_NAME, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kNamespaceTypeName, namespace_type_object));

  // Container
  PL_ASSIGN_OR_RETURN(auto container_type_object,
                      TypeObject::Create(types::STRING, types::ST_CONTAINER_NAME, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kContainerTypeName, container_type_object));

  // Bytes
  PL_ASSIGN_OR_RETURN(auto bytes_type_object,
                      TypeObject::Create(types::INT64, types::ST_BYTES, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kBytesTypeName, bytes_type_object));
  PL_ASSIGN_OR_RETURN(auto upid_type_object,
                      TypeObject::Create(types::UINT128, types::ST_UPID, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(PixieModule::kUPIDTypeName, upid_type_object));
  return Status::OK();
}

Status PixieModule::Init() {
  PL_RETURN_IF_ERROR(RegisterUDFFuncs());
  PL_RETURN_IF_ERROR(RegisterCompileTimeFuncs());
  PL_RETURN_IF_ERROR(RegisterUDTFs());
  PL_RETURN_IF_ERROR(RegisterTypeObjs());

  auto display_handler = func_based_exec_ ? &NoopDisplayHandler::Eval : DisplayHandler::Eval;
  // Setup methods.
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> display_fn,
      FuncObject::Create(kDisplayOpID, {"out", "name", "cols"},
                         {{"name", "'output'"}, {"cols", "[]"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(display_handler, graph_, compiler_state_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));

  AddMethod(kDisplayOpID, display_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> debug_fn,
      FuncObject::Create(
          kDebugOpID, {"out", "name", "cols"}, {{"name", "'output'"}, {"cols", "[]"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(DebugDisplayHandler::Eval, graph_, compiler_state_, reserved_names_,
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));

  AddMethod(kDebugOpID, debug_fn);

  PL_ASSIGN_OR_RETURN(auto base_df, Dataframe::Create(graph_, ast_visitor()));
  PL_RETURN_IF_ERROR(AssignAttribute(kDataframeOpID, base_df));
  PL_ASSIGN_OR_RETURN(auto viz, VisualizationObject::Create(ast_visitor()));
  return AssignAttribute(kVisAttrId, viz);
}

StatusOr<QLObjectPtr> DisplayHandler::Eval(IR* graph, CompilerState* compiler_state,
                                           const pypa::AstPtr& ast, const ParsedArgs& args,
                                           ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(OperatorIR * out_op, GetArgAs<OperatorIR>(args, "out"));
  PL_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(args, "name"));

  PL_RETURN_IF_ERROR(AddResultSink(graph, ast, name->str(), out_op,
                                   compiler_state->result_address(),
                                   compiler_state->result_ssl_targetname()));
  return StatusOr(std::make_shared<NoneObject>(visitor));
}

StatusOr<QLObjectPtr> NoopDisplayHandler::Eval(IR*, CompilerState*, const pypa::AstPtr&,
                                               const ParsedArgs&, ASTVisitor* visitor) {
  // TODO(PP-1773): Surface a warning to the user when calling px.display in a function based
  // execution regime. For now, we'll allow it and just have it do nothing.
  return StatusOr(std::make_shared<NoneObject>(visitor));
}

StatusOr<QLObjectPtr> DebugDisplayHandler::Eval(
    IR* graph, CompilerState* compiler_state,
    const absl::flat_hash_set<std::string>& reserved_names, const pypa::AstPtr& ast,
    const ParsedArgs& args, ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(OperatorIR * out_op, GetArgAs<OperatorIR>(args, "out"));
  PL_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(args, "name"));

  std::string out_name = PixieModule::kDebugTablePrefix + name->str();
  std::string out_name_base = out_name;
  // Remove ambiguitiy if there is repeated out_name in the display call and func_based_exec.
  int64_t i = 1;
  while (reserved_names.contains(out_name)) {
    out_name = absl::Substitute("$0_$1", out_name_base, i);
    ++i;
  }

  PL_RETURN_IF_ERROR(AddResultSink(graph, ast, out_name, out_op, compiler_state->result_address(),
                                   compiler_state->result_ssl_targetname()));
  return StatusOr(std::make_shared<NoneObject>(visitor));
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::NowEval(IR* graph, const pypa::AstPtr& ast,
                                                      const ParsedArgs&, ASTVisitor* visitor) {
  // TODO(philkuz/nserrino) maybe just convert this into an Integer because we have the info here.
  FuncIR::Op op{FuncIR::Opcode::non_op, "", PixieModule::kNowOpID};
  PL_ASSIGN_OR_RETURN(FuncIR * node,
                      graph->CreateNode<FuncIR>(ast, op, std::vector<ExpressionIR*>{}));
  return ExprObject::Create(node, visitor);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::TimeEval(IR* graph, std::string time_name,
                                                       const pypa::AstPtr& ast,
                                                       const ParsedArgs& args,
                                                       ASTVisitor* visitor) {
  // TODO(philkuz/nserrino) maybe just convert this into an Integer because we have the info here.
  std::vector<ExpressionIR*> expr_args;
  PL_ASSIGN_OR_RETURN(ExpressionIR * unit, GetArgAs<ExpressionIR>(args, "unit"));
  expr_args.push_back(unit);
  FuncIR::Op op{FuncIR::Opcode::non_op, "", time_name};
  PL_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, expr_args));
  return ExprObject::Create(node, visitor);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::UInt128Conversion(IR* graph, const pypa::AstPtr& ast,
                                                                const ParsedArgs& args,
                                                                ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(StringIR * uuid_str, GetArgAs<StringIR>(args, "uuid"));
  auto upid_or_s = md::UPID::ParseFromUUIDString(static_cast<StringIR*>(uuid_str)->str());
  if (!upid_or_s.ok()) {
    return uuid_str->CreateIRNodeError(upid_or_s.msg());
  }
  PL_ASSIGN_OR_RETURN(UInt128IR * uint128_ir,
                      graph->CreateNode<UInt128IR>(ast, upid_or_s.ConsumeValueOrDie().value()));

  return ExprObject::Create(uint128_ir, visitor);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::UPIDConstructor(IR* graph, const pypa::AstPtr& ast,
                                                              const ParsedArgs& args,
                                                              ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(IntIR * asid_ir, GetArgAs<IntIR>(args, "asid"));
  PL_ASSIGN_OR_RETURN(IntIR * pid_ir, GetArgAs<IntIR>(args, "pid"));
  PL_ASSIGN_OR_RETURN(IntIR * ts_ns_ir, GetArgAs<IntIR>(args, "ts_ns"));
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
  PL_ASSIGN_OR_RETURN(UInt128IR * uint128_ir, graph->CreateNode<UInt128IR>(ast, upid.value()));
  uint128_ir->SetTypeCast(ValueType::Create(uint128_ir->EvaluatedDataType(), types::ST_UPID));

  return ExprObject::Create(uint128_ir, visitor);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::AbsTime(IR* graph, const pypa::AstPtr& ast,
                                                      const ParsedArgs& args, ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(StringIR * date_str_ir, GetArgAs<StringIR>(args, "date_string"));
  PL_ASSIGN_OR_RETURN(StringIR * format_str_ir, GetArgAs<StringIR>(args, "format"));
  std::string date_str = date_str_ir->str();
  std::string format_str = format_str_ir->str();
  absl::Time tm;
  std::string err_str;
  if (!absl::ParseTime(format_str, date_str, &tm, &err_str)) {
    return CreateAstError(ast, "Failed to parse with error '$0'", err_str);
  }
  int64_t time_ns = absl::ToUnixNanos(tm);
  PL_ASSIGN_OR_RETURN(IntIR * time_count, graph->CreateNode<IntIR>(ast, time_ns));
  return StatusOr<QLObjectPtr>(ExprObject::Create(time_count, visitor));
}

StatusOr<QLObjectPtr> UDFHandler::Eval(IR* graph, std::string name, const pypa::AstPtr& ast,
                                       const ParsedArgs& args, ASTVisitor* visitor) {
  std::vector<ExpressionIR*> expr_args;
  for (const auto& arg : args.variable_args()) {
    if (!arg->HasNode()) {
      return CreateAstError(ast, "Argument to udf '$0' must be an expression", name);
    }
    auto ir_node = arg->node();
    if (!Match(ir_node, Expression())) {
      return ir_node->CreateIRNodeError("Argument must be an expression, got a $0",
                                        ir_node->type_string());
    }
    expr_args.push_back(static_cast<ExpressionIR*>(ir_node));
  }
  FuncIR::Op op{FuncIR::Opcode::non_op, "", name};
  PL_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, expr_args));
  return ExprObject::Create(node, visitor);
}

StatusOr<ExpressionIR*> UDTFSourceHandler::EvaluateExpression(
    IR* graph, IRNode* arg_node, const udfspb::UDTFSourceSpec::Arg& arg) {
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
                                           magic_enum::enum_name(arg.arg_type()),
                                           magic_enum::enum_name(data_node->EvaluatedDataType()));
      }
      return data_node;
    }
    case types::ST_UPID: {
      DCHECK_EQ(arg.arg_type(), types::UINT128);
      if (!Match(data_node, UInt128Value()) && !Match(data_node, String())) {
        return arg_node->CreateIRNodeError(
            "UPID must be a uint128 or str that converts to UUID, received a $0",
            magic_enum::enum_name(arg.arg_type()));
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

StatusOr<QLObjectPtr> UDTFSourceHandler::Eval(IR* graph,
                                              const udfspb::UDTFSourceSpec& udtf_source_spec,
                                              const pypa::AstPtr& ast, const ParsedArgs& args,
                                              ASTVisitor* visitor) {
  absl::flat_hash_map<std::string, ExpressionIR*> arg_map;
  for (const auto& arg : udtf_source_spec.args()) {
    DCHECK(args.args().contains(arg.name()));
    PL_ASSIGN_OR_RETURN(IRNode * arg_node, GetArgAs<IRNode>(args, arg.name()));
    PL_ASSIGN_OR_RETURN(arg_map[arg.name()], EvaluateExpression(graph, arg_node, arg));
  }
  PL_ASSIGN_OR_RETURN(
      UDTFSourceIR * udtf_source,
      graph->CreateNode<UDTFSourceIR>(ast, udtf_source_spec.name(), arg_map, udtf_source_spec));
  return Dataframe::Create(udtf_source, visitor);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
