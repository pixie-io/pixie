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

#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/ast/ast_visitor.h"
#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/metadata_object.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/time.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<Dataframe>> GetAsDataFrame(QLObjectPtr obj) {
  if (!Dataframe::IsDataframe(obj)) {
    return obj->CreateError("Expected DataFrame, received $0", obj->name());
  }
  return std::static_pointer_cast<Dataframe>(obj);
}

StatusOr<std::shared_ptr<Dataframe>> Dataframe::Create(CompilerState* compiler_state,
                                                       OperatorIR* op, ASTVisitor* visitor) {
  std::shared_ptr<Dataframe> df(new Dataframe(compiler_state, op, op->graph(), visitor));
  PX_RETURN_IF_ERROR(df->Init());
  return df;
}

StatusOr<std::shared_ptr<Dataframe>> Dataframe::Create(CompilerState* compiler_state, IR* graph,
                                                       ASTVisitor* visitor) {
  std::shared_ptr<Dataframe> df(new Dataframe(compiler_state, nullptr, graph, visitor));
  PX_RETURN_IF_ERROR(df->Init());
  return df;
}

// Parses elements of type TIRNodeType, either a single one or a collection, as a vector of
// TIRNodeType. Used for dataframe methods that take either a list or single item, like drop.
// drop('foo') and drop(['foo', 'bar']) for example.
template <typename TIRNodeType>
StatusOr<std::vector<TIRNodeType*>> ParseAsListOf(QLObjectPtr obj, std::string_view arg_name) {
  std::vector<TIRNodeType*> result;
  bool with_index = CollectionObject::IsCollection(obj);
  for (const auto& [idx, item] : Enumerate(ObjectAsCollection(obj))) {
    std::string name = std::string(arg_name);
    if (with_index) {
      name = absl::Substitute("$0 (index $1)", arg_name, idx);
    }
    PX_ASSIGN_OR_RETURN(auto casted, GetArgAs<TIRNodeType>(item, name));
    result.push_back(casted);
  }
  return result;
}

StatusOr<OperatorIR*> GetOperator(const QLObjectPtr& obj) {
  if (!Dataframe::IsDataframe(obj)) {
    return obj->CreateError("Expected 'DataFrame', got $0", obj->name());
  }
  return static_cast<Dataframe*>(obj.get())->op();
}

StatusOr<std::vector<OperatorIR*>> ParseAsListOfOperators(QLObjectPtr obj) {
  std::vector<OperatorIR*> result;
  for (const auto& [idx, item] : Enumerate(ObjectAsCollection(obj))) {
    PX_ASSIGN_OR_RETURN(auto casted, GetOperator(item));
    result.push_back(casted);
  }
  return result;
}

StatusOr<std::vector<std::string>> ParseAsListOfStrings(QLObjectPtr obj,
                                                        std::string_view arg_name) {
  PX_ASSIGN_OR_RETURN(auto string_irs, ParseAsListOf<StringIR>(obj, arg_name));
  std::vector<std::string> strs;
  strs.reserve(string_irs.size());
  for (StringIR* str : string_irs) {
    strs.push_back(str->str());
  }
  return strs;
}

/**
 * @brief Implements the DataFrame() constructor logic.
 */
StatusOr<QLObjectPtr> DataFrameConstructor(CompilerState* compiler_state, IR* graph,
                                           const pypa::AstPtr& ast, const ParsedArgs& args,
                                           ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StringIR * table, GetArgAs<StringIR>(ast, args, "table"));
  PX_ASSIGN_OR_RETURN(std::vector<std::string> columns,
                      ParseAsListOfStrings(args.GetArg("select"), "select"));
  std::string table_name = table->str();
  PX_ASSIGN_OR_RETURN(MemorySourceIR * mem_source_op,
                      graph->CreateNode<MemorySourceIR>(ast, table_name, columns));

  if (!NoneObject::IsNoneObject(args.GetArg("start_time"))) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * start_time, GetArgAs<ExpressionIR>(ast, args, "start_time"));
    PX_ASSIGN_OR_RETURN(auto start_time_ns,
                        ParseAllTimeFormats(compiler_state->time_now().val, start_time));
    mem_source_op->SetTimeStartNS(start_time_ns);
  }
  if (!NoneObject::IsNoneObject(args.GetArg("end_time"))) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * end_time, GetArgAs<ExpressionIR>(ast, args, "end_time"));
    PX_ASSIGN_OR_RETURN(auto end_time_ns,
                        ParseAllTimeFormats(compiler_state->time_now().val, end_time));
    mem_source_op->SetTimeStopNS(end_time_ns);
  }
  return Dataframe::Create(compiler_state, mem_source_op, visitor);
}

StatusOr<std::vector<ColumnIR*>> ProcessCols(IR* graph, const pypa::AstPtr& ast, QLObjectPtr obj,
                                             std::string arg_name, int64_t parent_index) {
  DCHECK(obj != nullptr);
  PX_ASSIGN_OR_RETURN(std::vector<std::string> column_names, ParseAsListOfStrings(obj, arg_name));
  std::vector<ColumnIR*> columns;
  columns.reserve(column_names.size());

  for (const auto& col_name : column_names) {
    PX_ASSIGN_OR_RETURN(ColumnIR * col, graph->CreateNode<ColumnIR>(ast, col_name, parent_index));
    columns.push_back(col);
  }
  return columns;
}

// Handles the merge() operator logic.
StatusOr<QLObjectPtr> JoinHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                  const pypa::AstPtr& ast, const ParsedArgs& args,
                                  ASTVisitor* visitor) {
  // GetArg returns non-nullptr or errors out in Debug mode. No need
  // to check again.
  PX_ASSIGN_OR_RETURN(OperatorIR * right, GetOperator(args.GetArg("right")));
  PX_ASSIGN_OR_RETURN(StringIR * how, GetArgAs<StringIR>(ast, args, "how"));
  QLObjectPtr suffixes_node = args.GetArg("suffixes");
  std::string how_type = how->str();

  PX_ASSIGN_OR_RETURN(std::vector<ColumnIR*> left_on_cols,
                      ProcessCols(graph, ast, args.GetArg("left_on"), "left_on", 0));
  PX_ASSIGN_OR_RETURN(std::vector<ColumnIR*> right_on_cols,
                      ProcessCols(graph, ast, args.GetArg("right_on"), "right_on", 1));

  // TODO(philkuz) consider using a struct instead of a vector because it's a fixed size.
  if (!CollectionObject::IsCollection(suffixes_node)) {
    return suffixes_node->CreateError(
        "'suffixes' must be a list with 2 strings for the left and right suffixes. Received $0",
        suffixes_node->name());
  }

  PX_ASSIGN_OR_RETURN(std::vector<std::string> suffix_strs,
                      ParseAsListOfStrings(suffixes_node, "suffixes"));
  if (suffix_strs.size() != 2) {
    return suffixes_node->CreateError("'suffixes' must be a list with 2 elements. Received $0",
                                      suffix_strs.size());
  }

  PX_ASSIGN_OR_RETURN(JoinIR * join_op,
                      graph->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{op, right}, how_type,
                                                left_on_cols, right_on_cols, suffix_strs));
  return Dataframe::Create(compiler_state, join_op, visitor);
}

StatusOr<FuncIR*> ParseNameTuple(IR* ir, const pypa::AstPtr& ast,
                                 std::shared_ptr<TupleObject> tuple) {
  DCHECK_GE(tuple->items().size(), 2UL);
  auto num_args = tuple->items().size() - 1;
  std::vector<StringIR*> arg_names;
  for (auto i = 0UL; i < num_args; i++) {
    auto name_or_s =
        GetArgAs<StringIR>(tuple->items()[i], absl::Substitute("$0-th tuple argument", i + 1));
    if (!name_or_s.ok()) {
      return tuple->items()[i]->CreateError(
          "All elements of the agg tuple must be column names, except the last which should be a "
          "function");
    }
    arg_names.push_back(name_or_s.ConsumeValueOrDie());
  }

  auto func = tuple->items()[num_args];
  if (func->type() != QLObjectType::kFunction) {
    return func->CreateError("Expected second tuple argument to be type Func, received $0",
                             func->name());
  }
  PX_ASSIGN_OR_RETURN(auto called, std::static_pointer_cast<FuncObject>(func)->Call({}, ast));

  PX_ASSIGN_OR_RETURN(FuncIR * func_ir, GetArgAs<FuncIR>(called, "last tuple argument"));

  // The function should be specified as a single function by itself.
  // This could change in the future.
  if (func_ir->all_args().size() != 0) {
    return func_ir->CreateIRNodeError("Unexpected aggregate function");
  }

  // parent_op_idx is 0 because we only have one parent for an aggregate.
  for (auto name : arg_names) {
    PX_ASSIGN_OR_RETURN(ColumnIR * argcol, ir->CreateNode<ColumnIR>(name->ast(), name->str(),
                                                                    /* parent_op_idx */ 0));
    PX_RETURN_IF_ERROR(func_ir->AddArg(argcol));
  }
  return func_ir;
}

StatusOr<QLObjectPtr> AggHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                 const pypa::AstPtr& ast, const ParsedArgs& args,
                                 ASTVisitor* visitor) {
  // converts the mapping of args.kwargs into ColExpressionvector
  ColExpressionVector aggregate_expressions;
  for (const auto& [name, expr_obj] : args.kwargs()) {
    if (expr_obj->type() != QLObjectType::kTuple) {
      return expr_obj->CreateError("Expected tuple for $0 but received $1", name, expr_obj->name());
    }
    PX_ASSIGN_OR_RETURN(
        FuncIR * parsed_expr,
        ParseNameTuple(graph, ast, std::static_pointer_cast<TupleObject>(expr_obj)));
    aggregate_expressions.push_back({name, parsed_expr});
  }

  PX_ASSIGN_OR_RETURN(
      BlockingAggIR * agg_op,
      graph->CreateNode<BlockingAggIR>(ast, op, std::vector<ColumnIR*>{}, aggregate_expressions));
  return Dataframe::Create(compiler_state, agg_op, visitor);
}

StatusOr<QLObjectPtr> MapAssignHandler(const pypa::AstPtr& ast, const ParsedArgs&, ASTVisitor*) {
  // TODO(philkuz) convert this to be an assign attribute.
  return CreateAstError(ast,
                        "Map assign not callable. use the attribute assignment syntax instead.");
}

// Handles the drop() DataFrame operator.
StatusOr<QLObjectPtr> DropHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                  const pypa::AstPtr& ast, const ParsedArgs& args,
                                  ASTVisitor* visitor) {
  QLObjectPtr columns_arg = args.GetArg("columns");
  PX_ASSIGN_OR_RETURN(std::vector<std::string> columns,
                      ParseAsListOfStrings(args.GetArg("columns"), "columns"));
  PX_ASSIGN_OR_RETURN(DropIR * drop_op, graph->CreateNode<DropIR>(ast, op, columns));
  return Dataframe::Create(compiler_state, drop_op, visitor);
}

// Handles the head() DataFrame logic.
StatusOr<QLObjectPtr> LimitHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                   const pypa::AstPtr& ast, const ParsedArgs& args,
                                   ASTVisitor* visitor) {
  // TODO(philkuz) (PL-1161) Add support for compile time evaluation of Limit argument.
  PX_ASSIGN_OR_RETURN(IntIR * rows_node, GetArgAs<IntIR>(ast, args, "n"));
  PX_ASSIGN_OR_RETURN(IntIR * pem_only, GetArgAs<IntIR>(ast, args, "_pem_only"));
  int64_t limit_value = rows_node->val();
  bool pem_only_val = pem_only->val() > 0;

  PX_ASSIGN_OR_RETURN(LimitIR * limit_op,
                      graph->CreateNode<LimitIR>(ast, op, limit_value, pem_only_val));
  return Dataframe::Create(compiler_state, limit_op, visitor);
}

class SubscriptHandler {
 public:
  /**
   * @brief Evaluates the subscript operator (filter and keep)
   */
  static StatusOr<QLObjectPtr> Eval(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                    const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);

 private:
  static StatusOr<QLObjectPtr> EvalFilter(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                          const pypa::AstPtr& ast, ExpressionIR* expr,
                                          ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> EvalKeep(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                        const pypa::AstPtr& ast, std::vector<StringIR*> keep_cols,
                                        ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> EvalColumn(IR* graph, OperatorIR* op, const pypa::AstPtr& ast,
                                          StringIR* cols, ASTVisitor* visitor);
};
StatusOr<QLObjectPtr> SubscriptHandler::Eval(CompilerState* compiler_state, IR* graph,
                                             OperatorIR* op, const pypa::AstPtr& ast,
                                             const ParsedArgs& args, ASTVisitor* visitor) {
  QLObjectPtr key = args.GetArg("key");
  if (CollectionObject::IsCollection(key)) {
    PX_ASSIGN_OR_RETURN(std::vector<StringIR*> keep_cols, ParseAsListOf<StringIR>(key, "key"));
    return EvalKeep(compiler_state, graph, op, ast, keep_cols, visitor);
  }
  if (!ExprObject::IsExprObject(key)) {
    return key->CreateError(
        "subscript argument must have a list of strings or expression. '$0' not allowed",
        key->name());
  }

  auto expr = static_cast<ExprObject*>(key.get());
  if (Match(expr->expr(), String())) {
    return EvalColumn(graph, op, ast, static_cast<StringIR*>(expr->expr()), visitor);
  }
  return EvalFilter(compiler_state, graph, op, ast, static_cast<ExpressionIR*>(expr->expr()),
                    visitor);
}

StatusOr<QLObjectPtr> SubscriptHandler::EvalFilter(CompilerState* compiler_state, IR* graph,
                                                   OperatorIR* op, const pypa::AstPtr& ast,
                                                   ExpressionIR* expr, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(FilterIR * filter_op, graph->CreateNode<FilterIR>(ast, op, expr));
  return Dataframe::Create(compiler_state, filter_op, visitor);
}

StatusOr<QLObjectPtr> SubscriptHandler::EvalColumn(IR* graph, OperatorIR*, const pypa::AstPtr&,
                                                   StringIR* expr, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(ColumnIR * column, graph->CreateNode<ColumnIR>(expr->ast(), expr->str(),
                                                                     /* parent_op_idx */ 0));
  return ExprObject::Create(column, visitor);
}

StatusOr<QLObjectPtr> SubscriptHandler::EvalKeep(CompilerState* compiler_state, IR* graph,
                                                 OperatorIR* op, const pypa::AstPtr& ast,
                                                 std::vector<StringIR*> keep_cols,
                                                 ASTVisitor* visitor) {
  absl::flat_hash_set<std::string> keep_cols_seen;
  ColExpressionVector keep_exprs;
  for (const auto& keep_col : keep_cols) {
    auto col_name = keep_col->str();
    if (keep_cols_seen.contains(col_name)) {
      return keep_col->CreateIRNodeError(
          "cannot specify the same column name more than once when filtering by cols. '$0' "
          "specified more than once",
          col_name);
    }
    keep_cols_seen.insert(col_name);

    // parent_op_idx is 0 because we only have one parent for a map.
    PX_ASSIGN_OR_RETURN(ColumnIR * column,
                        graph->CreateNode<ColumnIR>(ast, col_name, /* parent_op_idx */ 0));
    keep_exprs.emplace_back(col_name, column);
  }

  PX_ASSIGN_OR_RETURN(MapIR * map_op, graph->CreateNode<MapIR>(ast, op, keep_exprs,
                                                               /* keep_input_columns */ false));
  return Dataframe::Create(compiler_state, map_op, visitor);
}

// Handles the groupby() method.
StatusOr<QLObjectPtr> GroupByHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                     const pypa::AstPtr& ast, const ParsedArgs& args,
                                     ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(std::vector<std::string> group_names,
                      ParseAsListOfStrings(args.GetArg("by"), "by"));
  std::vector<ColumnIR*> groups;
  groups.reserve(group_names.size());
  for (const auto& group : group_names) {
    PX_ASSIGN_OR_RETURN(ColumnIR * col,
                        graph->CreateNode<ColumnIR>(ast, group, /* parent_idx */ 0));
    groups.push_back(col);
  }

  PX_ASSIGN_OR_RETURN(GroupByIR * group_by_op, graph->CreateNode<GroupByIR>(ast, op, groups));
  return Dataframe::Create(compiler_state, group_by_op, visitor);
}

// Handles the append() dataframe method and creates the union node.
StatusOr<QLObjectPtr> UnionHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                   const pypa::AstPtr& ast, const ParsedArgs& args,
                                   ASTVisitor* visitor) {
  std::vector<OperatorIR*> parents{op};
  for (const auto& [idx, item] : Enumerate(ObjectAsCollection(args.GetArg("objs")))) {
    PX_ASSIGN_OR_RETURN(auto casted, GetOperator(item));
    parents.push_back(casted);
  }
  PX_ASSIGN_OR_RETURN(UnionIR * union_op, graph->CreateNode<UnionIR>(ast, parents));
  return Dataframe::Create(compiler_state, union_op, visitor);
}

// Handles the rolling() dataframe method.
StatusOr<QLObjectPtr> RollingHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                     const pypa::AstPtr& ast, const ParsedArgs& args,
                                     ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StringIR * window_col_name, GetArgAs<StringIR>(ast, args, "on"));
  if (window_col_name->str() != "time_") {
    return window_col_name->CreateIRNodeError(
        "Windowing is only supported on time_ at the moment, not $0", window_col_name->str());
  }

  PX_ASSIGN_OR_RETURN(ExpressionIR * window_size_node, GetArgAs<ExpressionIR>(ast, args, "window"));
  // Set time_now to 0 because we don't need an offset time.
  PX_ASSIGN_OR_RETURN(auto window_size, ParseAllTimeFormats(/* time_now */ 0, window_size_node));
  if (window_size <= 0) {
    return window_size_node->CreateIRNodeError("Window size must be > 0");
  }

  PX_ASSIGN_OR_RETURN(ColumnIR * window_col,
                      graph->CreateNode<ColumnIR>(ast, window_col_name->str(), /* parent_idx */ 0));

  PX_ASSIGN_OR_RETURN(RollingIR * rolling_op,
                      graph->CreateNode<RollingIR>(ast, op, window_col, window_size));
  return Dataframe::Create(compiler_state, rolling_op, visitor);
}

/**
 * @brief Implements the stream() method and creates the stream node.
 *
 */
StatusOr<QLObjectPtr> StreamHandler(CompilerState* compiler_state, IR* graph, OperatorIR* op,
                                    const pypa::AstPtr& ast, const ParsedArgs&,
                                    ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(StreamIR * stream_op, graph->CreateNode<StreamIR>(ast, op));
  return Dataframe::Create(compiler_state, stream_op, visitor);
}

Status Dataframe::Init() {
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> constructor_fn,
      FuncObject::Create(
          name(), {"table", "select", "start_time", "end_time"},
          {{"select", "[]"}, {"start_time", "None"}, {"end_time", "None"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&DataFrameConstructor, compiler_state_, graph(), std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  AddCallMethod(constructor_fn);
  PX_RETURN_IF_ERROR(constructor_fn->SetDocString(kDataFrameConstuctorDocString));
  // If the op is null, then don't init the other funcs.
  if (op() == nullptr) {
    return Status::OK();
  }

  /**
   * # Equivalent to the python method method syntax:
   * def merge(self, right, how, left_on, right_on, suffixes=['_x', '_y']):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> mergefn,
      FuncObject::Create(
          kMergeOpID, {"right", "how", "left_on", "right_on", "suffixes"},
          {{"suffixes", "['_x', '_y']"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&JoinHandler, compiler_state_, graph(), op(), std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));

  PX_RETURN_IF_ERROR(mergefn->SetDocString(kMergeOpDocstring));
  AddMethod(kMergeOpID, mergefn);

  /**
   * # Equivalent to the python method method syntax:
   * def agg(self, **kwargs):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> aggfn,
                      FuncObject::Create(kBlockingAggOpID, {}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ true,
                                         std::bind(&AggHandler, compiler_state_, graph(), op(),
                                                   std::placeholders::_1, std::placeholders::_2,
                                                   std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(aggfn->SetDocString(kBlockingAggOpDocstring));
  AddMethod(kBlockingAggOpID, aggfn);

  // TODO(philkuz) temporary measure, need to fix the subscript assignment.
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> mapfn,
                      FuncObject::Create(kMapOpID, {"column_name", "expr"}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&MapAssignHandler, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(mapfn->SetDocString(kMapOpDocstring));
  AddMethod(kMapOpID, mapfn);
  /**
   * # Equivalent to the python method method syntax:
   * def drop(self, fn):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> dropfn,
      FuncObject::Create(
          kDropOpID, {"columns"}, {}, /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&DropHandler, compiler_state_, graph(), op(), std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  PX_RETURN_IF_ERROR(dropfn->SetDocString(kDropOpDocstring));
  AddMethod(kDropOpID, dropfn);

  /**
   * # Equivalent to the python method method syntax:
   * def head(self, n=5, _pem_only=False):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> limitfn,
      FuncObject::Create(
          kLimitOpID, {"n", "_pem_only"}, {{"n", "5"}, {"_pem_only", "0"}},
          /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&LimitHandler, compiler_state_, graph(), op(), std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  PX_RETURN_IF_ERROR(limitfn->SetDocString(kLimitOpDocstring));
  AddMethod(kLimitOpID, limitfn);

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
                     std::bind(&SubscriptHandler::Eval, compiler_state_, graph(), op(),
                               std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                     ast_visitor()));
  // TODO(philkuz) figure out how to combine these.
  PX_RETURN_IF_ERROR(subscript_fn->SetDocString(kKeepOpDocstring));
  AddSubscriptMethod(subscript_fn);
  // TODO(philkuz) temporary measure to make sure this is recorded.
  std::shared_ptr<FuncObject> filter_fn(
      new FuncObject(kFilterOpID, {"key"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&SubscriptHandler::Eval, compiler_state_, graph(), op(),
                               std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                     ast_visitor()));
  PX_RETURN_IF_ERROR(filter_fn->SetDocString(kFilterOpDocstring));
  AddMethod(kFilterOpID, filter_fn);

  std::shared_ptr<FuncObject> group_by_fn(
      new FuncObject(kGroupByOpID, {"by"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&GroupByHandler, compiler_state_, graph(), op(),
                               std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                     ast_visitor()));
  PX_RETURN_IF_ERROR(group_by_fn->SetDocString(kGroupByOpDocstring));
  AddMethod(kGroupByOpID, group_by_fn);

  /**
   * # Equivalent to the python method method syntax:
   * def append(self, fn):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> union_fn,
      FuncObject::Create(
          kUnionOpID, {"objs"}, {}, /* has_variable_len_args */ false,
          /* has_variable_len_kwargs */ false,
          std::bind(&UnionHandler, compiler_state_, graph(), op(), std::placeholders::_1,
                    std::placeholders::_2, std::placeholders::_3),
          ast_visitor()));
  PX_RETURN_IF_ERROR(union_fn->SetDocString(kUnionOpDocstring));
  AddMethod(kUnionOpID, union_fn);

  /**
   * # Equivalent to the python method syntax:
   * def rolling(self, window, on="time_"):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> rolling_fn,
                      FuncObject::Create(kRollingOpID, {"window", "on"}, {{"on", "'time_'"}},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&RollingHandler, compiler_state_, graph(), op(),
                                                   std::placeholders::_1, std::placeholders::_2,
                                                   std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(rolling_fn->SetDocString(kRollingOpDocstring));
  AddMethod(kRollingOpID, rolling_fn);

  /**
   * # Equivalent to the python method syntax:
   * def stream(self):
   *     ...
   */
  PX_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> stream_fn,
                      FuncObject::Create(kStreamOpId, {}, {},
                                         /* has_variable_len_args */ false,
                                         /* has_variable_len_kwargs */ false,
                                         std::bind(&StreamHandler, compiler_state_, graph(), op(),
                                                   std::placeholders::_1, std::placeholders::_2,
                                                   std::placeholders::_3),
                                         ast_visitor()));
  PX_RETURN_IF_ERROR(stream_fn->SetDocString(kStreamOpDocstring));
  AddMethod(kStreamOpId, stream_fn);

  PX_ASSIGN_OR_RETURN(auto md, MetadataObject::Create(op(), ast_visitor()));
  return AssignAttribute(kMetadataAttrName, md);
}

StatusOr<QLObjectPtr> Dataframe::GetAttributeImpl(const pypa::AstPtr& ast,
                                                  std::string_view name) const {
  // If this gets to this point, should fail here.
  DCHECK(HasNonMethodAttribute(name));

  if (QLObject::HasNonMethodAttribute(name)) {
    return QLObject::GetAttributeImpl(ast, name);
  }
  // We evaluate schemas in the analyzer, so at this point assume 'name' is a valid column.
  PX_ASSIGN_OR_RETURN(ColumnIR * column,
                      graph()->CreateNode<ColumnIR>(ast, std::string(name), /* parent_op_idx */ 0));
  return ExprObject::Create(column, ast_visitor());
}

StatusOr<std::shared_ptr<Dataframe>> Dataframe::FromColumnAssignment(CompilerState* compiler_state,
                                                                     const pypa::AstPtr& expr_node,
                                                                     ColumnIR* column,
                                                                     ExpressionIR* expr) {
  auto col_name = column->col_name();
  ColExpressionVector map_exprs{{col_name, expr}};
  PX_ASSIGN_OR_RETURN(MapIR * ir_node, graph_->CreateNode<MapIR>(expr_node, op(), map_exprs,
                                                                 /*keep_input_cols*/ true));
  return Dataframe::Create(compiler_state, ir_node, ast_visitor());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
