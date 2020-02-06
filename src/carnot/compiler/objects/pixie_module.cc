#include "src/carnot/compiler/objects/pixie_module.h"

#include <magic_enum.hpp>
#include <vector>

#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace compiler {
constexpr const char* const PixieModule::kTimeFuncs[];

StatusOr<std::shared_ptr<PixieModule>> PixieModule::Create(IR* graph, CompilerState* compiler_state,
                                                           const FlagValues& flag_values) {
  auto module = std::shared_ptr<PixieModule>(new PixieModule(graph, compiler_state));

  PL_RETURN_IF_ERROR(module->Init(flag_values));
  return module;
}

Status PixieModule::RegisterUDFFuncs() {
  // TODO(philkuz) (PL-1189) remove this when the udf names no longer have the 'pl.' prefix.
  for (const auto& name : compiler_state_->registry_info()->func_names()) {
    // attributes_.emplace(stripped_name);

    PL_ASSIGN_OR_RETURN(
        std::shared_ptr<FuncObject> fn_obj,
        FuncObject::Create(name, {}, {},
                           /* has_variable_len_args */ true,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&UDFHandler::Eval, graph_, std::string(name),
                                     std::placeholders::_1, std::placeholders::_2)));

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
                                     std::placeholders::_2)));

    AddMethod(udtf.name(), fn_obj);
  }
  return Status::OK();
}

Status PixieModule::RegisterCompileTimeFuncs() {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> now_fn,
      FuncObject::Create(kNowOpId, {}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&CompileTimeFuncHandler::NowEval, graph_, std::placeholders::_1,
                                   std::placeholders::_2)));
  AddMethod(kNowOpId, now_fn);
  for (const auto& time : kTimeFuncs) {
    PL_RETURN_IF_ERROR(RegisterCompileTimeUnitFunction(time));
  }

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> uuid_str_fn,
      FuncObject::Create(kUInt128ConversionId, {"uuid"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&CompileTimeFuncHandler::UInt128Conversion, graph_,
                                   std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kUInt128ConversionId, uuid_str_fn);
  return Status::OK();
}

Status PixieModule::RegisterCompileTimeUnitFunction(std::string name) {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> now_fn,
      FuncObject::Create(name, {"unit"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&CompileTimeFuncHandler::TimeEval, graph_, name,
                                   std::placeholders::_1, std::placeholders::_2)));
  AddMethod(name.data(), now_fn);
  return Status::OK();
}

Status PixieModule::Init(const FlagValues& flag_values) {
  // TODO(nserrino): Add in FlagsObject
  PL_UNUSED(flag_values);
  PL_RETURN_IF_ERROR(RegisterUDFFuncs());
  PL_RETURN_IF_ERROR(RegisterCompileTimeFuncs());
  PL_RETURN_IF_ERROR(RegisterUDTFs());

  // Setup methods.
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> display_fn,
      FuncObject::Create(
          kDisplayOpId, {"out", "name", "cols"}, {{"name", "'output'"}, {"cols", "[]"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&DisplayHandler::Eval, graph_, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kDisplayOpId, display_fn);

  attributes_.insert(kDataframeOpId);

  return Status::OK();
}

StatusOr<std::shared_ptr<QLObject>> PixieModule::GetAttributeImpl(const pypa::AstPtr& ast,
                                                                  std::string_view attr) const {
  if (attr == kDataframeOpId) {
    return Dataframe::Create(graph_);
  }
  return CreateAstError(ast, "'$0' has not attribute '$1'", name(), attr);
}

StatusOr<QLObjectPtr> DisplayHandler::Eval(IR* graph, const pypa::AstPtr& ast,
                                           const ParsedArgs& args) {
  PL_ASSIGN_OR_RETURN(OperatorIR * out_op, GetArgAs<OperatorIR>(args, "out"));
  PL_ASSIGN_OR_RETURN(StringIR * name, GetArgAs<StringIR>(args, "name"));

  std::string out_name = name->str();
  std::vector<std::string> columns;

  // TODO(PL-1197) support output columns in the analyzer rules.
  // PL_ASSIGN_OR_RETURN(IRNode* cols, GetArgAs<IRNode*>(args, "cols"));
  // if (!Match(cols, ListWithChildren(String()))) {
  //   return cols->CreateIRNodeError("'cols' must be a list of strings.");
  // }
  // PL_ASSIGN_OR_RETURN(std::vector<std::string> columns,
  //                     ParseStringsFromCollection(static_cast<ListIR*>(cols)));

  PL_RETURN_IF_ERROR(graph->CreateNode<MemorySinkIR>(ast, out_op, out_name, columns));
  return StatusOr(std::make_shared<NoneObject>());
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::NowEval(IR* graph, const pypa::AstPtr& ast,
                                                      const ParsedArgs&) {
  // TODO(philkuz/nserrino) maybe just convert this into an Integer because we have the info here.
  FuncIR::Op op{FuncIR::Opcode::non_op, "", PixieModule::kNowOpId};
  PL_ASSIGN_OR_RETURN(FuncIR * node,
                      graph->CreateNode<FuncIR>(ast, op, std::vector<ExpressionIR*>{}));
  return ExprObject::Create(node);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::TimeEval(IR* graph, std::string time_name,
                                                       const pypa::AstPtr& ast,
                                                       const ParsedArgs& args) {
  // TODO(philkuz/nserrino) maybe just convert this into an Integer because we have the info here.
  std::vector<ExpressionIR*> expr_args;
  PL_ASSIGN_OR_RETURN(ExpressionIR * unit, GetArgAs<ExpressionIR>(args, "unit"));
  expr_args.push_back(unit);
  FuncIR::Op op{FuncIR::Opcode::non_op, "", time_name};
  PL_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, expr_args));
  return ExprObject::Create(node);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::UInt128Conversion(IR* graph, const pypa::AstPtr& ast,
                                                                const ParsedArgs& args) {
  PL_ASSIGN_OR_RETURN(StringIR * uuid_str, GetArgAs<StringIR>(args, "uuid"));
  auto upid_or_s = md::UPID::ParseFromUUIDString(static_cast<StringIR*>(uuid_str)->str());
  if (!upid_or_s.ok()) {
    return uuid_str->CreateIRNodeError(upid_or_s.msg());
  }
  PL_ASSIGN_OR_RETURN(UInt128IR * uint128_ir,
                      graph->CreateNode<UInt128IR>(ast, upid_or_s.ConsumeValueOrDie().value()));

  return ExprObject::Create(uint128_ir);
}

StatusOr<QLObjectPtr> UDFHandler::Eval(IR* graph, std::string name, const pypa::AstPtr& ast,
                                       const ParsedArgs& args) {
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
  return ExprObject::Create(node);
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
        return graph->CreateNode<UInt128IR>(arg_node->ast_node(),
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
                                              const pypa::AstPtr& ast, const ParsedArgs& args) {
  absl::flat_hash_map<std::string, ExpressionIR*> arg_map;
  for (const auto& arg : udtf_source_spec.args()) {
    DCHECK(args.args().contains(arg.name()));
    PL_ASSIGN_OR_RETURN(IRNode * arg_node, GetArgAs<IRNode>(args, arg.name()));
    PL_ASSIGN_OR_RETURN(arg_map[arg.name()], EvaluateExpression(graph, arg_node, arg));
  }
  PL_ASSIGN_OR_RETURN(
      UDTFSourceIR * udtf_source,
      graph->CreateNode<UDTFSourceIR>(ast, udtf_source_spec.name(), arg_map, udtf_source_spec));
  return Dataframe::Create(udtf_source);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
