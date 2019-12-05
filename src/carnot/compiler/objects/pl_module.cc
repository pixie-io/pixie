#include <vector>

#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/pl_module.h"

namespace pl {
namespace carnot {
namespace compiler {
constexpr const char* const PLModule::kTimeFuncs[];

StatusOr<std::shared_ptr<PLModule>> PLModule::Create(IR* graph, CompilerState* compiler_state) {
  auto module = std::shared_ptr<PLModule>(new PLModule(graph, compiler_state));

  PL_RETURN_IF_ERROR(module->Init());
  return module;
}

Status PLModule::RegisterUDFFuncs() {
  // TODO(philkuz) (PL-1189) remove this when the udf names no longer have the 'pl.' prefix.
  for (const auto& name : compiler_state_->registry_info()->func_names()) {
    std::string_view stripped_name = absl::StripPrefix(name, "pl.");
    // attributes_.emplace(stripped_name);

    PL_ASSIGN_OR_RETURN(
        std::shared_ptr<FuncObject> fn_obj,
        FuncObject::Create(stripped_name, {}, {},
                           /* has_variable_len_args */ true,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&UDFHandler::Eval, graph_, stripped_name.data(),
                                     std::placeholders::_1, std::placeholders::_2)));

    AddMethod(stripped_name.data(), fn_obj);
  }
  // TODO(philkuz) (PL-1189) enable this.
  // attributes_ = compiler_state_->registry_info()->func_names()
  return Status::OK();
}

Status PLModule::RegisterCompileTimeFuncs() {
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
  return Status::OK();
}

Status PLModule::RegisterCompileTimeUnitFunction(std::string name) {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> now_fn,
      FuncObject::Create(name, {"unit"}, {},
                         /* has_variable_len_args */ false, /* has_variable_len_kwargs */ false,
                         std::bind(&CompileTimeFuncHandler::TimeEval, graph_, name,
                                   std::placeholders::_1, std::placeholders::_2)));
  AddMethod(name.data(), now_fn);
  return Status::OK();
}

Status PLModule::Init() {
  PL_RETURN_IF_ERROR(RegisterUDFFuncs());
  PL_RETURN_IF_ERROR(RegisterCompileTimeFuncs());

  // Setup methods.
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> dataframe_fn,
      FuncObject::Create(kDataframeOpId, {"table", "select", "start_time", "end_time"},
                         {{"select", "[]"}, {"start_time", "0"}, {"end_time", "pl.now()"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&DataFrameHandler::Eval, graph_, std::placeholders::_1,
                                   std::placeholders::_2)));
  AddMethod(kDataframeOpId, dataframe_fn);

  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> display_fn,
      FuncObject::Create(
          kDisplayOpId, {"out", "name", "cols"}, {{"name", "'output'"}, {"cols", "[]"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&DisplayHandler::Eval, graph_, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kDisplayOpId, display_fn);

  return Status::OK();
}

StatusOr<QLObjectPtr> PLModule::GetAttributeImpl(const pypa::AstPtr& ast,
                                                 const std::string& name) const {
  // If this gets to this point, should fail here.
  DCHECK(HasNonMethodAttribute(name));

  PL_ASSIGN_OR_RETURN(FuncIR * func,
                      graph_->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name},
                                                 std::vector<ExpressionIR*>{}));
  return ExprObject::Create(func);
}

StatusOr<QLObjectPtr> DataFrameHandler::Eval(IR* graph, const pypa::AstPtr& ast,
                                             const ParsedArgs& args) {
  IRNode* table = args.GetArg("table");
  IRNode* select = args.GetArg("select");
  IRNode* start_time = args.GetArg("start_time");
  IRNode* end_time = args.GetArg("end_time");
  if (!Match(table, String())) {
    return table->CreateIRNodeError("'table' must be a string, got $0", table->type_string());
  }

  if (!Match(select, ListWithChildren(String()))) {
    return select->CreateIRNodeError("'select' must be a list of strings.");
  }

  if (!start_time->IsExpression()) {
    return start_time->CreateIRNodeError("'start_time' must be an expression");
  }

  if (!end_time->IsExpression()) {
    return start_time->CreateIRNodeError("'end_time' must be an expression");
  }

  std::string table_name = static_cast<StringIR*>(table)->str();
  PL_ASSIGN_OR_RETURN(std::vector<std::string> columns,
                      ParseStringsFromCollection(static_cast<ListIR*>(select)));
  PL_ASSIGN_OR_RETURN(MemorySourceIR * mem_source_op,
                      graph->CreateNode<MemorySourceIR>(ast, table_name, columns));
  // If both start_time and end_time are default arguments, then we don't substitute them.
  if (!(args.default_subbed_args().contains("start_time") &&
        args.default_subbed_args().contains("end_time"))) {
    ExpressionIR* start_time_expr = static_cast<ExpressionIR*>(start_time);
    ExpressionIR* end_time_expr = static_cast<ExpressionIR*>(end_time);
    PL_RETURN_IF_ERROR(mem_source_op->SetTimeExpressions(start_time_expr, end_time_expr));
  }

  return Dataframe::Create(mem_source_op);
}

StatusOr<QLObjectPtr> DisplayHandler::Eval(IR* graph, const pypa::AstPtr& ast,
                                           const ParsedArgs& args) {
  IRNode* out = args.GetArg("out");
  IRNode* name = args.GetArg("name");

  if (!Match(out, Operator())) {
    return out->CreateIRNodeError("'out' must be a dataframe", out->type_string());
  }

  if (!Match(name, String())) {
    return name->CreateIRNodeError("'name' must be a string");
  }

  OperatorIR* out_op = static_cast<OperatorIR*>(out);
  std::string out_name = static_cast<StringIR*>(name)->str();
  std::vector<std::string> columns;

  // TODO(PL-1197) support output columns in the analyzer rules.
  // IRNode* cols = args.GetArg("cols");
  // if (!Match(cols, ListWithChildren(String()))) {
  //   return cols->CreateIRNodeError("'cols' must be a list of strings.");
  // }
  // PL_ASSIGN_OR_RETURN(std::vector<std::string> columns,
  //                     ParseStringsFromCollection(static_cast<ListIR*>(cols)));

  PL_ASSIGN_OR_RETURN(MemorySinkIR * mem_sink_op,
                      graph->CreateNode<MemorySinkIR>(ast, out_op, out_name, columns));
  return StatusOr(std::make_shared<NoneObject>(mem_sink_op));
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::NowEval(IR* graph, const pypa::AstPtr& ast,
                                                      const ParsedArgs&) {
  // TODO(philkuz/nserrino) maybe just convert this into an Integer because we have the info here.
  FuncIR::Op op{FuncIR::Opcode::non_op, "", PLModule::kNowOpId};
  PL_ASSIGN_OR_RETURN(FuncIR * node,
                      graph->CreateNode<FuncIR>(ast, op, std::vector<ExpressionIR*>{}));
  return ExprObject::Create(node);
}

StatusOr<QLObjectPtr> CompileTimeFuncHandler::TimeEval(IR* graph, const std::string& time_name,
                                                       const pypa::AstPtr& ast,
                                                       const ParsedArgs& args) {
  // TODO(philkuz/nserrino) maybe just convert this into an Integer because we have the info here.
  std::vector<ExpressionIR*> expr_args;
  IRNode* unit = args.GetArg("unit");
  if (!Match(unit, Expression())) {
    return unit->CreateIRNodeError("Argument must be an expression, got a $0", unit->type_string());
  }
  expr_args.push_back(static_cast<ExpressionIR*>(unit));
  FuncIR::Op op{FuncIR::Opcode::non_op, "", time_name};
  PL_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, expr_args));
  return ExprObject::Create(node);
}

StatusOr<QLObjectPtr> UDFHandler::Eval(IR* graph, const std::string& name, const pypa::AstPtr& ast,
                                       const ParsedArgs& args) {
  std::vector<ExpressionIR*> expr_args;
  for (const auto& arg : args.variable_args()) {
    if (!Match(arg, Expression())) {
      return arg->CreateIRNodeError("Argument must be an expression, got a $0", arg->type_string());
    }
    expr_args.push_back(static_cast<ExpressionIR*>(arg));
  }
  FuncIR::Op op{FuncIR::Opcode::non_op, "", name};
  PL_ASSIGN_OR_RETURN(FuncIR * node, graph->CreateNode<FuncIR>(ast, op, expr_args));
  return ExprObject::Create(node);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
