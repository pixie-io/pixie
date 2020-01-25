#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/ir/ast_utils.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/metadata_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/pixie_module.h"

namespace pl {
namespace carnot {
namespace compiler {
StatusOr<std::shared_ptr<Dataframe>> Dataframe::Create(OperatorIR* op) {
  std::shared_ptr<Dataframe> df(new Dataframe(op, op->graph_ptr()));
  PL_RETURN_IF_ERROR(df->Init());
  return df;
}

StatusOr<std::shared_ptr<Dataframe>> Dataframe::Create(IR* graph) {
  std::shared_ptr<Dataframe> df(new Dataframe(nullptr, graph));
  PL_RETURN_IF_ERROR(df->Init());
  return df;
}

Status Dataframe::Init() {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> constructor_fn,
      FuncObject::Create(name(), {"table", "select", "start_time", "end_time"},
                         {{"select", "[]"},
                          {"start_time", "0"},
                          {"end_time", absl::Substitute("$0.$1()", PixieModule::kPixieModuleObjName,
                                                        PixieModule::kNowOpId)}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&DataFrameHandler::Eval, graph(), std::placeholders::_1,
                                   std::placeholders::_2)));
  AddCallMethod(constructor_fn);
  // If the op is null, then don't init the other funcs.
  if (op() == nullptr) {
    return Status::OK();
  }

  /**
   * # Equivalent to the python method method syntax:
   * def merge(self, right, how, left_on, right_on, suffixes=('_x', '_y')):
   *     ...
   */
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> mergefn,
      FuncObject::Create(kMergeOpId, {"right", "how", "left_on", "right_on", "suffixes"},
                         {{"suffixes", "('_x', '_y')"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&JoinHandler::Eval, graph(), op(), std::placeholders::_1,
                                   std::placeholders::_2)));
  AddMethod(kMergeOpId, mergefn);

  /**
   * # Equivalent to the python method method syntax:
   * def agg(self, **kwargs):
   *     ...
   */
  PL_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> aggfn,
                      FuncObject::Create(kBlockingAggOpId, {}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ true,
                                         std::bind(&AggHandler::Eval, graph(), op(),
                                                   std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kBlockingAggOpId, aggfn);

  /**
   * # Equivalent to the python method method syntax:
   * def drop(self, fn):
   *     ...
   */
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> dropfn,
      FuncObject::Create(kDropOpId, {"columns"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&DropHandler::Eval, graph(), op(), std::placeholders::_1,
                                   std::placeholders::_2)));
  AddMethod(kDropOpId, dropfn);

  /**
   * # Equivalent to the python method method syntax:
   * def head(self, n=5):
   *     ...
   */
  PL_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> limitfn,
                      FuncObject::Create(kLimitOpId, {"n"}, {{"n", "5"}},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&LimitHandler::Eval, graph(), op(),
                                                   std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kLimitOpId, limitfn);

  /**
   *
   * # Equivalent to the python method method syntax:
   * def __getitem__(self, key):
   *     ...
   *
   * # It's important to note that this is added as a subscript method instead.
   */
  std::shared_ptr<FuncObject> subscript_fn(
      new FuncObject(kSubscriptMethodName, {"key"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&SubscriptHandler::Eval, graph(), op(), std::placeholders::_1,
                               std::placeholders::_2)));
  AddSubscriptMethod(subscript_fn);

  std::shared_ptr<FuncObject> group_by_fn(
      new FuncObject(kGroupByOpId, {"by"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&GroupByHandler::Eval, graph(), op(), std::placeholders::_1,
                               std::placeholders::_2)));
  AddMethod(kGroupByOpId, group_by_fn);

  /**
   * # Equivalent to the python method method syntax:
   * def append(self, fn):
   *     ...
   */
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> union_fn,
      FuncObject::Create(kUnionOpId, {"objs"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&UnionHandler::Eval, graph(), op(), std::placeholders::_1,
                                   std::placeholders::_2)));
  AddMethod(kUnionOpId, union_fn);

  attributes_.emplace(kMetadataAttrName);
  return Status::OK();
}

StatusOr<QLObjectPtr> Dataframe::GetAttributeImpl(const pypa::AstPtr& ast,
                                                  std::string_view name) const {
  // If this gets to this point, should fail here.
  DCHECK(HasNonMethodAttribute(name));

  if (name == kMetadataAttrName) {
    return MetadataObject::Create(op());
  }

  // We evaluate schemas in the analyzer, so at this point assume 'name' is a valid column.
  PL_ASSIGN_OR_RETURN(ColumnIR * column,
                      graph()->CreateNode<ColumnIR>(ast, std::string(name), /* parent_op_idx */ 0));
  return ExprObject::Create(column);
}

StatusOr<QLObjectPtr> JoinHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                        const ParsedArgs& args) {
  // GetArg returns non-nullptr or errors out in Debug mode. No need
  // to check again.
  IRNode* right_node = args.GetArg("right");
  IRNode* how_node = args.GetArg("how");
  IRNode* left_on_node = args.GetArg("left_on");
  IRNode* right_on_node = args.GetArg("right_on");
  IRNode* suffixes_node = args.GetArg("suffixes");
  if (!Match(right_node, Operator())) {
    return right_node->CreateIRNodeError("'right' must be an operator, got $0",
                                         right_node->type_string());
  }
  OperatorIR* right = static_cast<OperatorIR*>(right_node);

  if (!Match(how_node, String())) {
    return how_node->CreateIRNodeError("'how' must be a string, got $0", how_node->type_string());
  }
  std::string how_type = static_cast<StringIR*>(how_node)->str();

  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> left_on_cols, ProcessCols(left_on_node, "left_on", 0));
  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> right_on_cols,
                      ProcessCols(right_on_node, "right_on", 1));

  // TODO(philkuz) consider using a struct instead of a vector because it's a fixed size.
  if (!Match(suffixes_node, CollectionWithChildren(String()))) {
    return suffixes_node->CreateIRNodeError(
        "'suffixes' must be a list with 2 strings for the left and right suffixes. Received $0",
        suffixes_node->type_string());
  }

  PL_ASSIGN_OR_RETURN(std::vector<std::string> suffix_strs,
                      ParseStringsFromCollection(static_cast<ListIR*>(suffixes_node)));
  if (suffix_strs.size() != 2) {
    return suffixes_node->CreateIRNodeError(
        "'suffixes' must be a list with 2 elements. Received $0", suffix_strs.size());
  }

  PL_ASSIGN_OR_RETURN(JoinIR * join_op,
                      graph->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{op, right}, how_type,
                                                left_on_cols, right_on_cols, suffix_strs));
  return Dataframe::Create(join_op);
}

StatusOr<std::vector<ColumnIR*>> JoinHandler::ProcessCols(IRNode* node, std::string arg_name,
                                                          int64_t parent_index) {
  DCHECK(node != nullptr);
  IR* graph = node->graph_ptr();
  if (Match(node, ListWithChildren(String()))) {
    auto list = static_cast<ListIR*>(node);
    std::vector<ColumnIR*> columns(list->children().size());
    for (const auto& [idx, node] : Enumerate(list->children())) {
      StringIR* str = static_cast<StringIR*>(node);
      PL_ASSIGN_OR_RETURN(ColumnIR * col,
                          graph->CreateNode<ColumnIR>(str->ast_node(), str->str(), parent_index));
      columns[idx] = col;
    }
    return columns;
  } else if (!Match(node, String())) {
    return node->CreateIRNodeError("'$0' must be a label or a list of labels", arg_name);
  }
  StringIR* str = static_cast<StringIR*>(node);
  PL_ASSIGN_OR_RETURN(ColumnIR * col,
                      graph->CreateNode<ColumnIR>(str->ast_node(), str->str(), parent_index));
  return std::vector<ColumnIR*>{col};
}

StatusOr<QLObjectPtr> AggHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                       const ParsedArgs& args) {
  // converts the mapping of args.kwargs into ColExpressionvector
  ColExpressionVector aggregate_expressions;
  for (const auto& [name, expr] : args.kwargs()) {
    if (!Match(expr, Tuple())) {
      return expr->CreateIRNodeError("Expected '$0' kwarg argument to be a tuple, not $1",
                                     Dataframe::kBlockingAggOpId, expr->type_string());
    }
    PL_ASSIGN_OR_RETURN(FuncIR * parsed_expr, ParseNameTuple(graph, static_cast<TupleIR*>(expr)));
    aggregate_expressions.push_back({name, parsed_expr});
  }

  PL_ASSIGN_OR_RETURN(
      BlockingAggIR * agg_op,
      graph->CreateNode<BlockingAggIR>(ast, op, std::vector<ColumnIR*>{}, aggregate_expressions));
  return Dataframe::Create(agg_op);
}

StatusOr<FuncIR*> AggHandler::ParseNameTuple(IR* ir, TupleIR* tuple) {
  DCHECK_EQ(tuple->children().size(), 2UL);
  IRNode* childone = tuple->children()[0];
  IRNode* childtwo = tuple->children()[1];
  if (!Match(childone, String())) {
    return childone->CreateIRNodeError("Expected 'str' for first tuple argument. Received '$0'",
                                       childone->type_string());
  }

  if (!Match(childtwo, Func())) {
    return childtwo->CreateIRNodeError("Expected 'func' for second tuple argument. Received '$0'",
                                       childtwo->type_string());
  }

  std::string argcol_name = static_cast<StringIR*>(childone)->str();
  FuncIR* func = static_cast<FuncIR*>(childtwo);
  // The function should be specified as a single function by itself.
  // This could change in the future.
  if (func->args().size() != 0) {
    return func->CreateIRNodeError("Unexpected aggregate function");
  }
  // parent_op_idx is 0 because we only have one parent for an aggregate.
  PL_ASSIGN_OR_RETURN(ColumnIR * argcol, ir->CreateNode<ColumnIR>(childone->ast_node(), argcol_name,
                                                                  /* parent_op_idx */ 0));
  PL_RETURN_IF_ERROR(func->AddArg(argcol));

  // Delete tuple id.
  PL_RETURN_IF_ERROR(ir->DeleteNode(tuple->id()));
  return func;
}

StatusOr<QLObjectPtr> DropHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                        const ParsedArgs& args) {
  IRNode* columns_arg = args.GetArg("columns");
  std::vector<std::string> columns;
  if (Match(columns_arg, String())) {
    columns.push_back(static_cast<StringIR*>(columns_arg)->str());
  } else if (Match(columns_arg, List())) {
    ListIR* columns_list = static_cast<ListIR*>(columns_arg);
    PL_ASSIGN_OR_RETURN(columns, ParseStringsFromCollection(columns_list));
  } else {
    return columns_arg->CreateIRNodeError(
        "Expected '$0' kwarg argument 'columns' to be a list or a string, not $1",
        Dataframe::kDropOpId, columns_arg->type_string());
  }

  PL_ASSIGN_OR_RETURN(DropIR * drop_op, graph->CreateNode<DropIR>(ast, op, columns));
  return Dataframe::Create(drop_op);
}

StatusOr<QLObjectPtr> LimitHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                         const ParsedArgs& args) {
  // TODO(philkuz) (PL-1161) Add support for compile time evaluation of Limit argument.
  IRNode* rows_node = args.GetArg("n");
  if (!Match(rows_node, Int())) {
    return rows_node->CreateIRNodeError("'n' must be an int");
  }
  int64_t limit_value = static_cast<IntIR*>(rows_node)->val();

  PL_ASSIGN_OR_RETURN(LimitIR * limit_op, graph->CreateNode<LimitIR>(ast, op, limit_value));
  // Delete the integer node.
  PL_RETURN_IF_ERROR(graph->DeleteNode(rows_node->id()));
  return Dataframe::Create(limit_op);
}

StatusOr<QLObjectPtr> SubscriptHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                             const ParsedArgs& args) {
  IRNode* key = args.GetArg("key");
  if (Match(key, String())) {
    return EvalColumn(graph, op, ast, static_cast<StringIR*>(key));
  }
  if (Match(key, ListWithChildren(String()))) {
    return EvalKeep(graph, op, ast, static_cast<ListIR*>(key));
  }
  if (key->IsExpression()) {
    return EvalFilter(graph, op, ast, static_cast<ExpressionIR*>(key));
  }
  return key->CreateIRNodeError(
      "subscript argument must have a list of strings or expression. '$0' not allowed",
      key->type_string());
}

StatusOr<QLObjectPtr> SubscriptHandler::EvalFilter(IR* graph, OperatorIR* op,
                                                   const pypa::AstPtr& ast, ExpressionIR* expr) {
  PL_ASSIGN_OR_RETURN(FilterIR * filter_op, graph->CreateNode<FilterIR>(ast, op, expr));
  return Dataframe::Create(filter_op);
}

StatusOr<QLObjectPtr> SubscriptHandler::EvalColumn(IR* graph, OperatorIR*, const pypa::AstPtr&,
                                                   StringIR* expr) {
  PL_ASSIGN_OR_RETURN(ColumnIR * column, graph->CreateNode<ColumnIR>(expr->ast_node(), expr->str(),
                                                                     /* parent_op_idx */ 0));
  return ExprObject::Create(column);
}

StatusOr<QLObjectPtr> SubscriptHandler::EvalKeep(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                                 ListIR* key) {
  PL_ASSIGN_OR_RETURN(std::vector<std::string> keep_column_names, ParseStringsFromCollection(key));

  ColExpressionVector keep_exprs;
  for (const auto& col_name : keep_column_names) {
    // parent_op_idx is 0 because we only have one parent for a map.
    PL_ASSIGN_OR_RETURN(ColumnIR * keep_col,
                        graph->CreateNode<ColumnIR>(ast, col_name, /* parent_op_idx */ 0));
    keep_exprs.emplace_back(col_name, keep_col);
  }

  PL_ASSIGN_OR_RETURN(MapIR * map_op, graph->CreateNode<MapIR>(ast, op, keep_exprs,
                                                               /* keep_input_columns */ false));
  return Dataframe::Create(map_op);
}

StatusOr<QLObjectPtr> GroupByHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                           const ParsedArgs& args) {
  IRNode* by = args.GetArg("by");

  PL_ASSIGN_OR_RETURN(std::vector<ColumnIR*> groups, ParseByFunction(by));
  PL_ASSIGN_OR_RETURN(GroupByIR * group_by_op, graph->CreateNode<GroupByIR>(ast, op, groups));
  return Dataframe::Create(group_by_op);
}

StatusOr<std::vector<ColumnIR*>> GroupByHandler::ParseByFunction(IRNode* by) {
  if (!Match(by, ListWithChildren(String())) && !Match(by, String())) {
    return by->CreateIRNodeError("'by' expected string or list of strings");
  } else if (Match(by, String())) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col,
                        by->graph_ptr()->CreateNode<ColumnIR>(
                            by->ast_node(), static_cast<StringIR*>(by)->str(), /* parent_idx */ 0));
    return std::vector<ColumnIR*>{col};
  }

  PL_ASSIGN_OR_RETURN(std::vector<std::string> column_names,
                      ParseStringsFromCollection(static_cast<ListIR*>(by)));
  std::vector<ColumnIR*> columns;
  for (const auto& col_name : column_names) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col,
                        by->graph_ptr()->CreateNode<ColumnIR>(by->ast_node(), col_name,
                                                              /* parent_idx */ 0));
    columns.push_back(col);
  }
  return columns;
}

StatusOr<QLObjectPtr> UnionHandler::Eval(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                         const ParsedArgs& args) {
  IRNode* objs_arg = args.GetArg("objs");

  std::vector<OperatorIR*> parents{op};
  if (Match(objs_arg, Operator())) {
    parents.push_back(static_cast<OperatorIR*>(objs_arg));
  } else if (Match(objs_arg, ListWithChildren(Operator()))) {
    for (auto child : static_cast<ListIR*>(objs_arg)->children()) {
      parents.push_back(static_cast<OperatorIR*>(child));
    }
  } else {
    return objs_arg->CreateIRNodeError(
        "Expected '$0' kwarg argument 'objs' to be a dataframe or a list of dataframes, not $1",
        Dataframe::kUnionOpId, objs_arg->type_string());
  }

  PL_ASSIGN_OR_RETURN(UnionIR * union_op, graph->CreateNode<UnionIR>(ast, parents));
  return Dataframe::Create(union_op);
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
