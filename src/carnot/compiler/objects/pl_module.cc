#include <vector>

#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/pl_module.h"

namespace pl {
namespace carnot {
namespace compiler {

StatusOr<std::shared_ptr<PLModule>> PLModule::Create(IR* graph, CompilerState* compiler_state) {
  auto module = std::shared_ptr<PLModule>(new PLModule(graph, compiler_state));

  PL_RETURN_IF_ERROR(module->Init());
  return module;
}

Status PLModule::Init() {
  // TODO(philkuz) (PL-1189) remove this when the udf names no longer have the 'pl.' prefix.
  for (const auto& name : compiler_state_->registry_info()->func_names()) {
    attributes_.emplace(absl::StripPrefix(name, "pl."));
  }
  // TODO(philkuz) (PL-1189) enable this.
  // attributes_ = compiler_state_->registry_info()->func_names()

  // Setup methods.
  std::shared_ptr<FuncObject> dataframe_fn = std::shared_ptr<FuncObject>(new FuncObject(
      kDataframeOpId, {"table", "select", "start_time", "end_time"},
      {{"select", "[]"}, {"start_time", "0"}, {"end_time", "pl.now()"}},
      /*has_variable_len_kwargs*/ false,
      std::bind(&DataFrameHandler::Eval, graph_, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kDataframeOpId, dataframe_fn);

  std::shared_ptr<FuncObject> display_fn = std::shared_ptr<FuncObject>(new FuncObject(
      kDisplayOpId, {"out", "name", "cols"}, {{"name", "''"}, {"cols", "[]"}},
      /*has_variable_len_kwargs*/ false,
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
