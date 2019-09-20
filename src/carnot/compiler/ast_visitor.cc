#include "src/carnot/compiler/ast_visitor.h"

#include "src/carnot/compiler/compiler_error_context.h"
#include "src/carnot/compiler/pattern_match.h"

namespace pl {
namespace carnot {
namespace compiler {
using pypa::AstType;

const std::unordered_map<std::string, std::chrono::nanoseconds> kUnitTimeFnStr = {
    {"minutes", std::chrono::minutes(1)},           {"hours", std::chrono::hours(1)},
    {"seconds", std::chrono::seconds(1)},           {"days", std::chrono::hours(24)},
    {"microseconds", std::chrono::microseconds(1)}, {"milliseconds", std::chrono::milliseconds(1)}};

StatusOr<FuncIR::Op> ASTWalker::GetOp(const std::string& python_op, const pypa::AstPtr node) {
  auto op_find = FuncIR::op_map.find(python_op);
  if (op_find == FuncIR::op_map.end()) {
    return CreateAstError(node, "Operator '$0' not handled", python_op);
  }
  return op_find->second;
}

ASTWalker::ASTWalker(std::shared_ptr<IR> ir_graph, CompilerState* compiler_state) {
  ir_graph_ = ir_graph;
  var_table_ = VarTable();
  compiler_state_ = compiler_state;
}

template <typename... Args>
Status ASTWalker::CreateAstError(const pypa::AstPtr& ast, Args... args) {
  compilerpb::CompilerErrorGroup context =
      LineColErrorPb(ast->line, ast->column, absl::Substitute(args...));
  return Status(statuspb::INVALID_ARGUMENT, "",
                std::make_unique<compilerpb::CompilerErrorGroup>(context));
}

template <typename... Args>
Status ASTWalker::CreateAstError(const pypa::Ast& ast, Args... args) {
  return CreateAstError(std::make_shared<pypa::Ast>(ast), args...);
}

std::string ASTWalker::GetAstTypeName(pypa::AstType type) {
  std::vector<std::string> type_names = {
#undef PYPA_AST_TYPE
#define PYPA_AST_TYPE(X) #X,
// NOLINTNEXTLINE(build/include_order).
#include <pypa/ast/ast_type.inl>
#undef PYPA_AST_TYPE
  };
  DCHECK(type_names.size() > static_cast<size_t>(type));
  return absl::Substitute("$0", type_names[static_cast<int>(type)]);
}

Status ASTWalker::ProcessExprStmtNode(const pypa::AstExpressionStatementPtr& e) {
  switch (e->expr->type) {
    case AstType::Call:
      return ProcessOpCallNode(PYPA_PTR_CAST(Call, e->expr)).status();
    default:
      return CreateAstError(e, "Expression node not defined");
  }
}

Status ASTWalker::ProcessModuleNode(const pypa::AstModulePtr& m) {
  pypa::AstStmtList items_list = m->body->items;
  // iterate through all the items on this list.
  std::vector<Status> status_vector;
  for (pypa::AstStmt stmt : items_list) {
    switch (stmt->type) {
      case pypa::AstType::ExpressionStatement: {
        Status result = ProcessExprStmtNode(PYPA_PTR_CAST(ExpressionStatement, stmt));
        if (!result.ok()) {
          status_vector.push_back(result);
        }
        break;
      }
      case pypa::AstType::Assign: {
        Status result = ProcessAssignNode(PYPA_PTR_CAST(Assign, stmt));
        if (!result.ok()) {
          status_vector.push_back(result);
        }
        break;
      }
      default: {
        status_vector.push_back(
            CreateAstError(m, "Can't parse expression of type $0", GetAstTypeName(stmt->type)));
      }
    }
  }
  return MergeStatuses(status_vector);
}

Status ASTWalker::ProcessAssignNode(const pypa::AstAssignPtr& node) {
  // Check # nodes to assign.
  if (node->targets.size() != 1) {
    return CreateAstError(node, "AssignNodes are only supported with one target.");
  }
  // Get the name that we are targeting.
  auto expr_node = node->targets[0];
  if (expr_node->type != AstType::Name) {
    return CreateAstError(expr_node, "Assign target must be a Name node.");
  }
  std::string assign_name = GetNameID(expr_node);
  // Get the object that we want to assign.
  if (node->value->type != AstType::Call) {
    return CreateAstError(node->value, "Assign value must be a function call.");
  }
  PL_ASSIGN_OR_RETURN(var_table_[assign_name], ProcessOpCallNode(PYPA_PTR_CAST(Call, node->value)));
  return Status::OK();
}

StatusOr<std::string> ASTWalker::GetFuncName(const pypa::AstCallPtr& node) {
  std::string func_name;
  switch (node->function->type) {
    case AstType::Name: {
      func_name = GetNameID(node->function);
      break;
    }
    case AstType::Attribute: {
      auto attr = PYPA_PTR_CAST(Attribute, node->function);
      if (attr->attribute->type != AstType::Name) {
        return CreateAstError(node->function, "Couldn't get string name out of node of type $0.",
                              GetAstTypeName(attr->attribute->type));
      }
      func_name = GetNameID(attr->attribute);
      break;
    }
    default: {
      return CreateAstError(node->function, "Couldn't get string name out of node of type $0.",
                            GetAstTypeName(node->function->type));
    }
  }
  return func_name;
}

StatusOr<ArgMap> ASTWalker::ProcessArgs(const pypa::AstCallPtr& call_ast,
                                        const OperatorContext& op_context,
                                        const std::vector<std::string>& expected_args,
                                        bool kwargs_only) {
  return ProcessArgs(call_ast, op_context, expected_args, kwargs_only, {{}});
}

StatusOr<ArgMap> ASTWalker::ProcessArgs(
    const pypa::AstCallPtr& call_ast, const OperatorContext& op_context,
    const std::vector<std::string>& expected_args, bool kwargs_only,
    const std::unordered_map<std::string, IRNode*>& default_args) {
  auto arg_ast = call_ast->arglist;
  if (!kwargs_only) {
    return error::Unimplemented("Only supporting kwargs for now.");
  }
  ArgMap arg_map;
  // Set to keep track of args that are not yet found.
  std::unordered_set<std::string> missing_or_default_args;
  missing_or_default_args.insert(expected_args.begin(), expected_args.end());

  std::vector<Status> errors;
  // Iterate through the keywords
  for (auto& k : arg_ast.keywords) {
    pypa::AstKeywordPtr kw_ptr = PYPA_PTR_CAST(Keyword, k);
    std::string key = GetNameID(kw_ptr->name);
    if (missing_or_default_args.find(key) == missing_or_default_args.end()) {
      errors.push_back(CreateAstError(call_ast, "Keyword '$0' not expected in function.", key));
      continue;
    }
    missing_or_default_args.erase(missing_or_default_args.find(key));
    PL_ASSIGN_OR_RETURN(IRNode * value, ProcessData(kw_ptr->value, op_context));
    arg_map[key] = value;
  }

  for (const auto& ma : missing_or_default_args) {
    auto find_ma = default_args.find(ma);
    if (find_ma == default_args.end()) {
      // TODO(philkuz) look for places where ast error might exit prematurely in other parts of
      // the code.
      errors.push_back(
          CreateAstError(call_ast, "You must set '$0' directly. No default value found.", ma));
      continue;
    }
    arg_map[ma] = find_ma->second;
  }
  PL_RETURN_IF_ERROR(MergeStatuses(errors));
  return arg_map;
}

StatusOr<OperatorIR*> ASTWalker::LookupName(const pypa::AstNamePtr& name_node) {
  // if doesn't exist, then
  auto find_name = var_table_.find(name_node->id);
  if (find_name == var_table_.end()) {
    return CreateAstError(name_node, "Can't find variable '$0'.", name_node->id);
  }
  IRNode* node = find_name->second;
  if (!node->IsOperator()) {
    return node->CreateIRNodeError(
        "Can only have operators saved as variables for now. Can't handle $0.",
        node->type_string());
  }
  return static_cast<OperatorIR*>(node);
}

StatusOr<OperatorIR*> ASTWalker::ProcessAttribute(const pypa::AstAttributePtr& node) {
  switch (node->value->type) {
    case AstType::Call: {
      return ProcessOpCallNode(PYPA_PTR_CAST(Call, node->value));
    }
    case AstType::Name: {
      return LookupName(PYPA_PTR_CAST(Name, node->value));
    }
    default: {
      return CreateAstError(node->value, "Can't handle the attribute of this type");
    }
  }
}

template <typename TOpIR>
StatusOr<TOpIR*> ASTWalker::ProcessOp(const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(TOpIR * ir_node, ir_graph_->MakeNode<TOpIR>());
  if (!(ir_node->is_source()) && node->function->type != AstType::Attribute) {
    return CreateAstError(
        node->function,
        "Only source operators can be called from outside a table reference. '$0' "
        "needs to be called as an attribute of a table.",
        ir_node->type_string());
  }
  OperatorIR* parent_op = nullptr;
  if (node->function->type == AstType::Attribute) {
    PL_ASSIGN_OR_RETURN(parent_op, ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  }
  PL_ASSIGN_OR_RETURN(ArgMap args,
                      ProcessArgs(node, OperatorContext({parent_op}, ir_node), ir_node->ArgKeys(),
                                  true, ir_node->DefaultArgValues(node)));

  PL_RETURN_IF_ERROR(ir_node->Init(parent_op, args, node));
  return ir_node;
}

template <>
StatusOr<RangeIR*> ASTWalker::ProcessOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(node->function, "Expected Range to be an attribute, not a $0",
                          GetAstTypeName(node->function->type));
  }
  PL_ASSIGN_OR_RETURN(RangeIR * ir_node, ir_graph_->MakeNode<RangeIR>());
  // Setup the default arguments.
  PL_ASSIGN_OR_RETURN(IntIR * default_stop_node, MakeTimeNow(node));

  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));

  PL_ASSIGN_OR_RETURN(
      ArgMap args, ProcessArgs(node, OperatorContext({parent_op}, ir_node), {"start", "stop"}, true,
                               {{"stop", default_stop_node}}));
  PL_RETURN_IF_ERROR(ir_node->Init(parent_op, args["start"], args["stop"], node));
  return ir_node;
}

template <>
StatusOr<JoinIR*> ASTWalker::ProcessOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(node->function, "Expected Join to be an attribute, not a $0",
                          GetAstTypeName(node->function->type));
  }
  PL_ASSIGN_OR_RETURN(JoinIR * ir_node, ir_graph_->MakeNode<JoinIR>());

  OperatorIR* parent_op1 = nullptr;
  if (node->function->type == AstType::Attribute) {
    PL_ASSIGN_OR_RETURN(parent_op1, ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  }
  auto arg_list = node->arglist;

  std::string expected_first_arg_string =
      "Expected first argument of Join operator to be a Name referencing a "
      "parent";
  if (arg_list.arguments.size() < 1) {
    return CreateAstError(node, "$0. No argument specified.", expected_first_arg_string);
  }

  if (arg_list.arguments.size() > 1) {
    return CreateAstError(node, "$0. Got more than one argument.", expected_first_arg_string);
  }

  auto parent_arg = arg_list.arguments[0];
  if (parent_arg->type != AstType::Name) {
    return CreateAstError(parent_arg, "$0. Got $1 instead.", expected_first_arg_string,
                          GetAstTypeName(parent_arg->type));
  }

  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op2, LookupName(PYPA_PTR_CAST(Name, parent_arg)));

  PL_ASSIGN_OR_RETURN(ArgMap args,
                      ProcessArgs(node, OperatorContext({parent_op1, parent_op2}, ir_node),
                                  ir_node->ArgKeys(), true, ir_node->DefaultArgValues(node)));

  PL_RETURN_IF_ERROR(ir_node->Init({parent_op1, parent_op2}, args, node));
  return ir_node;
}

StatusOr<OperatorIR*> ASTWalker::ProcessOpCallNode(const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(std::string func_name, GetFuncName(node));
  OperatorIR* ir_node;
  if (func_name == kFromOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<MemorySourceIR>(node));
  } else if (func_name == kRangeOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<RangeIR>(node));
  } else if (func_name == kMapOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<MapIR>(node));
  } else if (func_name == kFilterOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<FilterIR>(node));
  } else if (func_name == kLimitOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<LimitIR>(node));
  } else if (func_name == kBlockingAggOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<BlockingAggIR>(node));
  } else if (func_name == kSinkOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<MemorySinkIR>(node));
  } else if (func_name == kJoinOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<JoinIR>(node));
  } else if (func_name == kRangeAggOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessRangeAggOp(node));
  } else {
    return CreateAstError(node, "No function named '$0'", func_name);
  }
  return ir_node;
}

StatusOr<OperatorIR*> ASTWalker::ProcessRangeAggOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(node->function, "Expected RangeAgg to be an attribute, not a $0",
                          GetAstTypeName(node->function->type));
  }

  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));

  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, OperatorContext({parent_op}, kRangeAggOpId),
                                               {"fn", "by", "size"}, true));

  // Create Map IR.
  PL_ASSIGN_OR_RETURN(MapIR * map_ir_node, ir_graph_->MakeNode<MapIR>());

  // pl.mod(by_col, size).
  DCHECK(args["by"]->type() == IRNodeType::kLambda);
  LambdaIR* by_lambda = static_cast<LambdaIR*>(args["by"]);
  if (by_lambda->col_exprs().size() > 1) {
    return by_lambda->CreateIRNodeError(
        "Too many arguments for by argument of RangeAgg, only 1 is supported. ");
  }

  IRNode* by_expr = by_lambda->col_exprs()[0].node;
  if (Match(by_expr, List())) {
    ListIR* by_list = static_cast<ListIR*>(by_expr);
    if (by_list->children().size() != 1) {
      return by_list->CreateIRNodeError(
          "$0 supports a single group by column, please update the query.", kRangeAggOpId);
    }

    by_expr = by_list->children()[0];
    IR* ir = by_expr->graph_ptr();
    ir->dag().DeleteEdge(by_lambda->id(), by_list->id());
    ir->dag().DeleteEdge(by_list->id(), by_expr->id());
    ir->dag().DeleteNode(by_list->id());
    PL_RETURN_IF_ERROR(ir->AddEdge(by_lambda, by_expr));
    LOG(INFO) << by_lambda->DebugString();
    LOG(INFO) << by_expr->DebugString();
  }

  if (!Match(by_expr, ColumnNode())) {
    return by_expr->CreateIRNodeError("Group-by argument must be a Column, not a %0",
                                      by_expr->type_string());
  }

  ColumnIR* by_col = static_cast<ColumnIR*>(by_expr);

  PL_RETURN_IF_ERROR(ir_graph_->DeleteEdge(by_lambda->id(), by_col->id()));
  PL_ASSIGN_OR_RETURN(FuncIR * mod_ir_node, ir_graph_->MakeNode<FuncIR>());
  PL_ASSIGN_OR_RETURN(FuncIR::Op mod_op, GetOp("%", node));
  DCHECK(args["size"]->IsExpression());
  PL_RETURN_IF_ERROR(mod_ir_node->Init(
      mod_op, kRunTimeFuncPrefix,
      std::vector<ExpressionIR*>({by_col, static_cast<ExpressionIR*>(args["size"])}),
      false /*compile_time */, node));

  PL_ASSIGN_OR_RETURN(ColumnIR * by_col_copy, ir_graph_->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(
      by_col_copy->Init(by_col->col_name(), /* parent_op_idx */ 0, by_col->ast_node()));
  // pl.subtract(by_col, pl.mod(by_col, size)).
  PL_ASSIGN_OR_RETURN(FuncIR * sub_ir_node, ir_graph_->MakeNode<FuncIR>());
  PL_ASSIGN_OR_RETURN(FuncIR::Op sub_op, GetOp("-", node));
  PL_RETURN_IF_ERROR(sub_ir_node->Init(sub_op, kRunTimeFuncPrefix,
                                       std::vector<ExpressionIR*>({by_col_copy, mod_ir_node}),
                                       false /*compile_time */, node));

  // Map(lambda r: {'group': pl.subtract(by_col, pl.modulo(by_col, size))}.
  PL_ASSIGN_OR_RETURN(LambdaIR * map_lambda_ir_node, ir_graph_->MakeNode<LambdaIR>());
  // Pull in all columns needed in fn.
  ColExpressionVector map_exprs = ColExpressionVector({ColumnExpression{"group", sub_ir_node}});
  for (const auto& name : static_cast<LambdaIR*>(args["fn"])->expected_column_names()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col_node, ir_graph_->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(col_node->Init(name, /* parent_op_idx */ 0, node));
    map_exprs.push_back(ColumnExpression{name, col_node});
  }
  PL_RETURN_IF_ERROR(map_lambda_ir_node->Init(std::unordered_set<std::string>({by_col->col_name()}),
                                              map_exprs, node));
  ArgMap map_args{{"fn", map_lambda_ir_node}};
  PL_RETURN_IF_ERROR(map_ir_node->Init(parent_op, map_args, node));

  // Create BlockingAggIR
  PL_ASSIGN_OR_RETURN(BlockingAggIR * agg_ir_node, ir_graph_->MakeNode<BlockingAggIR>());

  // by = lambda r: r.group.
  PL_ASSIGN_OR_RETURN(ColumnIR * agg_col_ir_node, ir_graph_->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(agg_col_ir_node->Init("group", /* parent_op_idx*/ 0, node));
  PL_ASSIGN_OR_RETURN(LambdaIR * agg_by_ir_node, ir_graph_->MakeNode<LambdaIR>());
  PL_RETURN_IF_ERROR(
      agg_by_ir_node->Init(std::unordered_set<std::string>({"group"}), agg_col_ir_node, node));

  // Agg(fn = fn, by = lambda r: r.group).
  IRNode* fn_node = args["fn"];
  ArgMap agg_args{{"by", agg_by_ir_node}, {"fn", fn_node}};
  PL_RETURN_IF_ERROR(agg_ir_node->Init(map_ir_node, agg_args, node));
  return agg_ir_node;
}

StatusOr<ExpressionIR*> ASTWalker::ProcessStr(const pypa::AstStrPtr& ast) {
  PL_ASSIGN_OR_RETURN(StringIR * ir_node, ir_graph_->MakeNode<StringIR>());
  PL_ASSIGN_OR_RETURN(auto str_value, GetStrAstValue(ast));
  PL_RETURN_IF_ERROR(ir_node->Init(str_value, ast));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessList(const pypa::AstListPtr& ast,
                                         const OperatorContext& op_context) {
  ListIR* ir_node = ir_graph_->MakeNode<ListIR>().ValueOrDie();
  std::vector<ExpressionIR*> children;
  for (auto& child : ast->elements) {
    PL_ASSIGN_OR_RETURN(IRNode * child_node, ProcessData(child, op_context));
    if (!child_node->IsExpression()) {
      return CreateAstError(ast, "Can't support '$0' as a List member.", child_node->type_string());
    }
    children.push_back(static_cast<ExpressionIR*>(child_node));
  }
  PL_RETURN_IF_ERROR(ir_node->Init(ast, children));
  return ir_node;
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaNestedAttribute(
    const LambdaOperatorMap& arg_op_map, const std::string& attribute_value,
    const pypa::AstAttributePtr& parent_attr) {
  // The nested attribute only works with the format:
  // "<parent_attr.value>.<parent_attr.attribute>.<attribute_value>"

  // The following 3 checks make sure that parent_attr is "<value>.<attribute>".
  auto value = parent_attr->value;
  auto attribute = parent_attr->attribute;
  if (value->type != AstType::Name) {
    return CreateAstError(value, "Expected a Name here, got a $0", GetAstTypeName(value->type));
  }

  if (attribute->type != AstType::Name) {
    return CreateAstError(attribute, "Expected a Name here, got a $0",
                          GetAstTypeName(attribute->type));
  }
  std::string value_name = GetNameID(value);
  if (value_name != kCompileTimeFuncPrefix && value_name != kRunTimeFuncPrefix &&
      arg_op_map.find(value_name) == arg_op_map.end()) {
    return CreateAstError(value, "Name '$0' not defined.", value_name);
  }

  std::vector<std::string> expected_attrs = {kMDKeyword};
  // Find handled attribute keywords.
  if (GetNameID(attribute) == kMDKeyword) {
    // If attribute is the metadata keyword, then we process the metadata attribute.
    return ProcessMetadataAttribute(arg_op_map, attribute_value, parent_attr);
  }

  return CreateAstError(attribute, "Nested attribute can only be one of [$0]. '$1' not supported",
                        absl::StrJoin(expected_attrs, ","), GetNameID(attribute));
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessMetadataAttribute(
    const LambdaOperatorMap& arg_op_map, const std::string& attribute_value,
    const pypa::AstAttributePtr& val_attr) {
  std::string value = GetNameID(val_attr->value);

  auto arg_op_iter = arg_op_map.find(value);
  if (arg_op_iter == arg_op_map.end()) {
    return CreateAstError(val_attr,
                          "Metadata call not allowed on '$0'. Must use a lambda argument "
                          "to access Metadata.",
                          value);
  }
  PL_ASSIGN_OR_RETURN(MetadataIR * md_node, ir_graph_->MakeNode<MetadataIR>());
  int64_t parent_op_idx = arg_op_iter->second;
  PL_RETURN_IF_ERROR(md_node->Init(attribute_value, parent_op_idx, val_attr->attribute));
  return LambdaExprReturn(md_node, {attribute_value});
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaAttribute(const LambdaOperatorMap& arg_op_map,
                                                             const pypa::AstAttributePtr& node) {
  // Attributes in libpypa follow the format: "<parent_value>.<attribute>".
  // Attribute should always be a name, but just in case it's not, we error out.
  if (node->attribute->type != AstType::Name) {
    return CreateAstError(node->attribute, "Attribute must be a Name not a $0.",
                          GetAstTypeName(node->attribute->type));
  }

  // Values of attributes (the parent of the attribute) can also be attributes themselves.
  // Nested attributes are handled here.
  if (node->value->type == AstType::Attribute) {
    return ProcessLambdaNestedAttribute(arg_op_map, GetNameID(node->attribute),
                                        PYPA_PTR_CAST(Attribute, node->value));
  }

  // If value is not an attribute, then it must be a name.
  if (node->value->type != AstType::Name) {
    return CreateAstError(node->value, "Attribute parent must be a name or an attribute, not a $0.",
                          GetAstTypeName(node->value->type));
  }

  std::string parent = GetNameID(node->value);
  if (parent == kRunTimeFuncPrefix) {
    // If the value of the attribute is kRunTimeFuncPrefix, then we have a function without
    // arguments.
    return ProcessArglessFunction(kRunTimeFuncPrefix, GetNameID(node->attribute));
  } else if (parent == kCompileTimeFuncPrefix) {
    // TODO(philkuz) should spend time figuring out how to make this work, logically it should.
    return CreateAstError(node, "'$0' not supported in lambda functions.", kCompileTimeFuncPrefix);
  } else if (arg_op_map.find(parent) != arg_op_map.end()) {
    // If the value is equal to the arg_op_map, then the attribute is a column.
    int64_t parent_op_idx = arg_op_map.find(parent)->second;
    return ProcessRecordColumn(GetNameID(node->attribute), node, parent_op_idx);
  }
  return CreateAstError(node, "Name '$0' is not defined.", parent);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessRecordColumn(const std::string& column_name,
                                                          const pypa::AstPtr& column_ast_node,
                                                          int64_t parent_op_idx) {
  std::unordered_set<std::string> column_names;
  column_names.insert(column_name);
  PL_ASSIGN_OR_RETURN(ColumnIR * column, ir_graph_->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(column->Init(column_name, parent_op_idx, column_ast_node));
  return LambdaExprReturn(column, column_names);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessArglessFunction(const std::string& function_prefix,
                                                             const std::string& function_name) {
  PL_UNUSED(function_prefix);
  // TODO(philkuz) (PL-733) use this to remove the is_pixie_attr from LambdaExprReturn.
  return LambdaExprReturn(function_name, true /*is_pixie_attr */);
}

StatusOr<ExpressionIR*> ASTWalker::ProcessNumber(const pypa::AstNumberPtr& node) {
  switch (node->num_type) {
    case pypa::AstNumber::Type::Float: {
      PL_ASSIGN_OR_RETURN(FloatIR * ir_node, ir_graph_->MakeNode<FloatIR>());
      PL_RETURN_IF_ERROR(ir_node->Init(node->floating, node));
      return ir_node;
    }
    case pypa::AstNumber::Type::Integer:
    case pypa::AstNumber::Type::Long: {
      PL_ASSIGN_OR_RETURN(IntIR * ir_node, ir_graph_->MakeNode<IntIR>());
      PL_RETURN_IF_ERROR(ir_node->Init(node->integer, node));
      return ir_node;
    }
    default:
      return CreateAstError(node, "Couldn't find number type $0", node->num_type);
  }
}

StatusOr<LambdaExprReturn> ASTWalker::BuildLambdaFunc(
    const FuncIR::Op& op, const std::string& prefix,
    const std::vector<LambdaExprReturn>& children_ret_expr, const pypa::AstPtr& parent_node) {
  PL_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->MakeNode<FuncIR>());
  std::vector<ExpressionIR*> expressions;
  auto ret = LambdaExprReturn(ir_node);
  for (auto expr_ret : children_ret_expr) {
    if (expr_ret.StringOnly()) {
      return CreateAstError(parent_node, "Attribute call with '$0' prefix not allowed in lambda.",
                            kCompileTimeFuncPrefix);
    } else {
      expressions.push_back(expr_ret.expr_);
      ret.MergeColumns(expr_ret);
    }
  }
  PL_RETURN_IF_ERROR(ir_node->Init(op, prefix, expressions, false /*compile_time */, parent_node));
  return ret;
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaBinOp(const LambdaOperatorMap& arg_op_map,
                                                         const pypa::AstBinOpPtr& node) {
  std::string op_str = pypa::to_string(node->op);
  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<LambdaExprReturn> children_ret_expr;
  PL_ASSIGN_OR_RETURN(auto left_expr_ret, ProcessLambdaExpr(arg_op_map, node->left));
  PL_ASSIGN_OR_RETURN(auto right_expr_ret, ProcessLambdaExpr(arg_op_map, node->right));
  children_ret_expr.push_back(left_expr_ret);
  children_ret_expr.push_back(right_expr_ret);
  return BuildLambdaFunc(op, kRunTimeFuncPrefix, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaBoolOp(const LambdaOperatorMap& arg_op_map,
                                                          const pypa::AstBoolOpPtr& node) {
  std::string op_str = pypa::to_string(node->op);
  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<LambdaExprReturn> children_ret_expr;
  for (auto comp : node->values) {
    PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_op_map, comp));
    children_ret_expr.push_back(rt);
  }
  return BuildLambdaFunc(op, kRunTimeFuncPrefix, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaCompare(const LambdaOperatorMap& arg_op_map,
                                                           const pypa::AstComparePtr& node) {
  DCHECK_EQ(node->operators.size(), 1ULL);
  std::string op_str = pypa::to_string(node->operators[0]);
  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<LambdaExprReturn> children_ret_expr;
  if (node->comparators.size() != 1) {
    return CreateAstError(node, "Only expected one argument to the right of '$0'.", op_str);
  }
  PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_op_map, node->left));
  children_ret_expr.push_back(rt);
  for (auto comp : node->comparators) {
    PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_op_map, comp));
    children_ret_expr.push_back(rt);
  }
  return BuildLambdaFunc(op, kRunTimeFuncPrefix, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaCall(const LambdaOperatorMap& arg_op_map,
                                                        const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(auto attr_result, ProcessLambdaExpr(arg_op_map, node->function));
  if (!attr_result.StringOnly()) {
    return CreateAstError(node, "Expected a string for the return");
  }
  auto arglist = node->arglist;
  if (!arglist.defaults.empty() || !arglist.keywords.empty()) {
    return CreateAstError(node, "Only non-default and non-keyword args allowed.");
  }

  std::string fn_name = attr_result.str_;
  std::vector<LambdaExprReturn> children_ret_expr;
  for (auto arg_ast : arglist.arguments) {
    PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_op_map, arg_ast));
    children_ret_expr.push_back(rt);
  }
  FuncIR::Op op{FuncIR::Opcode::non_op, "", fn_name};
  return BuildLambdaFunc(op, kRunTimeFuncPrefix, children_ret_expr, node);
}

/**
 * @brief Wraps an IrNode StatusOr return with the LambdaExprReturn StatusOr
 *
 * @param node
 * @return StatusOr<LambdaExprReturn>
 */
StatusOr<LambdaExprReturn> WrapLambdaExprReturn(StatusOr<ExpressionIR*> node) {
  PL_RETURN_IF_ERROR(node);
  return LambdaExprReturn(node.ValueOrDie());
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaList(const LambdaOperatorMap& arg_op_map,
                                                        const pypa::AstListPtr& node) {
  ListIR* ir_node = ir_graph_->MakeNode<ListIR>().ValueOrDie();
  LambdaExprReturn expr_return(ir_node);
  std::vector<ExpressionIR*> children;
  for (auto& child : node->elements) {
    PL_ASSIGN_OR_RETURN(auto child_attr, ProcessLambdaExpr(arg_op_map, child));
    if (child_attr.StringOnly() || (!child_attr.expr_->IsColumn())) {
      return CreateAstError(
          node, "Lambda list can only handle Metadata and Column types. Can't handle $0.",
          child_attr.expr_->type_string());
    }
    expr_return.MergeColumns(child_attr);
    children.push_back(child_attr.expr_);
  }
  PL_RETURN_IF_ERROR(ir_node->Init(node, children));
  return expr_return;
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaExpr(const LambdaOperatorMap& arg_op_map,
                                                        const pypa::AstPtr& node) {
  LambdaExprReturn expr_return;
  switch (node->type) {
    case AstType::BinOp: {
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaBinOp(arg_op_map, PYPA_PTR_CAST(BinOp, node)));
      break;
    }
    case AstType::Attribute: {
      PL_ASSIGN_OR_RETURN(expr_return,
                          ProcessLambdaAttribute(arg_op_map, PYPA_PTR_CAST(Attribute, node)));
      break;
    }
    case AstType::Number: {
      auto number_result = ProcessNumber(PYPA_PTR_CAST(Number, node));
      PL_ASSIGN_OR_RETURN(expr_return, WrapLambdaExprReturn(number_result));
      break;
    }
    case AstType::Str: {
      auto str_result = ProcessStr(PYPA_PTR_CAST(Str, node));
      PL_ASSIGN_OR_RETURN(expr_return, WrapLambdaExprReturn(str_result));
      break;
    }
    case AstType::Call: {
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaCall(arg_op_map, PYPA_PTR_CAST(Call, node)));
      break;
    }
    case AstType::List: {
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaList(arg_op_map, PYPA_PTR_CAST(List, node)));
      break;
    }
    case AstType::Compare: {
      PL_ASSIGN_OR_RETURN(expr_return,
                          ProcessLambdaCompare(arg_op_map, PYPA_PTR_CAST(Compare, node)));
      break;
    }
    case AstType::BoolOp: {
      PL_ASSIGN_OR_RETURN(expr_return,
                          ProcessLambdaBoolOp(arg_op_map, PYPA_PTR_CAST(BoolOp, node)));
      break;
    }
    default: {
      return CreateAstError(node, "Node of type $0 not allowed for expression in Lambda function.",
                            GetAstTypeName(node->type));
    }
  }
  return expr_return;
}

StatusOr<LambdaOperatorMap> ASTWalker::ProcessLambdaArgs(const pypa::AstLambdaPtr& node,
                                                         const OperatorContext& op_context) {
  auto arg_ast = node->arguments;
  if (!arg_ast.defaults.empty() && arg_ast.defaults[0]) {
    return CreateAstError(node, "No default arguments allowed for lambdas. Found $0 default args.",
                          arg_ast.defaults.size());
  }
  if (!arg_ast.keywords.empty()) {
    return CreateAstError(node, "No keyword arguments allowed for lambdas.");
  }
  auto parent_ops = op_context.parent_ops;

  if (arg_ast.arguments.size() != parent_ops.size()) {
    return CreateAstError(node, "Got $0 lambda arguments, expected $1 for the $2 Operator.",
                          arg_ast.arguments.size(), parent_ops.size(), op_context.operator_name);
  }
  LambdaOperatorMap out_map;
  for (size_t i = 0; i < parent_ops.size(); ++i) {
    auto arg_node = arg_ast.arguments[i];
    if (arg_node->type != AstType::Name) {
      return CreateAstError(node, "Argument must be a Name.");
    }
    std::string arg_str = GetNameID(arg_node);
    if (out_map.find(arg_str) != out_map.end()) {
      return CreateAstError(node, "Duplicate argument '$0' in lambda definition.", arg_str);
    }

    out_map.emplace(arg_str, /* parent_op_idx */ i);
  }
  return out_map;
}

StatusOr<LambdaBodyReturn> ASTWalker::ProcessLambdaDict(const LambdaOperatorMap& arg_op_map,
                                                        const pypa::AstDictPtr& body_dict) {
  auto return_val = LambdaBodyReturn();
  for (size_t i = 0; i < body_dict->keys.size(); i++) {
    auto key_str_ast = body_dict->keys[i];
    auto value_ast = body_dict->values[i];
    PL_ASSIGN_OR_RETURN(auto key_string, GetStrAstValue(key_str_ast));
    PL_ASSIGN_OR_RETURN(auto expr_ret, ProcessLambdaExpr(arg_op_map, value_ast));
    if (expr_ret.is_pixie_attr_) {
      PL_ASSIGN_OR_RETURN(expr_ret, BuildLambdaFunc({FuncIR::Opcode::non_op, "", expr_ret.str_},
                                                    kRunTimeFuncPrefix, {}, body_dict));
    }
    PL_RETURN_IF_ERROR(return_val.AddExprResult(key_string, expr_ret));
  }
  return return_val;
}

StatusOr<LambdaIR*> ASTWalker::ProcessLambda(const pypa::AstLambdaPtr& ast,
                                             const OperatorContext& op_context) {
  LambdaIR* lambda_node = ir_graph_->MakeNode<LambdaIR>().ValueOrDie();
  PL_ASSIGN_OR_RETURN(LambdaOperatorMap arg_op_map, ProcessLambdaArgs(ast, op_context));
  LambdaBodyReturn return_struct;
  switch (ast->body->type) {
    case AstType::Dict: {
      PL_ASSIGN_OR_RETURN(return_struct,
                          ProcessLambdaDict(arg_op_map, PYPA_PTR_CAST(Dict, ast->body)));
      PL_RETURN_IF_ERROR(lambda_node->Init(return_struct.input_relation_columns_,
                                           return_struct.col_exprs_, ast->body));
      return lambda_node;
    }

    default: {
      PL_ASSIGN_OR_RETURN(LambdaExprReturn return_val, ProcessLambdaExpr(arg_op_map, ast->body));
      PL_RETURN_IF_ERROR(
          lambda_node->Init(return_val.input_relation_columns_, return_val.expr_, ast->body));
      return lambda_node;
    }
  }
}

StatusOr<IntIR*> ASTWalker::MakeTimeNow(const pypa::AstPtr& ast_node) {
  PL_ASSIGN_OR_RETURN(IntIR * ir_node, ir_graph_->MakeNode<IntIR>());
  PL_RETURN_IF_ERROR(ir_node->Init(compiler_state_->time_now().val, ast_node));
  return ir_node;
}

StatusOr<IntIR*> ASTWalker::EvalCompileTimeNow(const pypa::AstArguments& arglist,
                                               const pypa::AstPtr& arglist_parent) {
  if (!arglist.arguments.empty() || !arglist.keywords.empty()) {
    return CreateAstError(arglist_parent, "No Arguments expected for '$0.$1' fn",
                          kCompileTimeFuncPrefix, "now");
  }
  PL_ASSIGN_OR_RETURN(IntIR * ir_node, MakeTimeNow(arglist_parent));
  return ir_node;
}

StatusOr<IntIR*> ASTWalker::EvalUnitTimeFn(const std::string& attr_fn_name,
                                           const pypa::AstArguments& arglist,
                                           const pypa::AstPtr& arglist_parent) {
  auto fn_type_iter = kUnitTimeFnStr.find(attr_fn_name);
  if (fn_type_iter == kUnitTimeFnStr.end()) {
    return CreateAstError(arglist_parent, "Function '$0.$1' not found", kCompileTimeFuncPrefix,
                          attr_fn_name);
  }
  if (!arglist.keywords.empty()) {
    return CreateAstError(arglist_parent, "No Keyword args expected for '$0.$1' fn",
                          kCompileTimeFuncPrefix, attr_fn_name);
  }
  if (arglist.arguments.size() != 1) {
    return CreateAstError(arglist_parent, "Only expected one arg for '$0.$1' fn",
                          kCompileTimeFuncPrefix, attr_fn_name);
  }
  auto argument = arglist.arguments[0];
  if (argument->type != AstType::Number) {
    return CreateAstError(arglist_parent, "Argument must be an integer for '$0.$1'",
                          kCompileTimeFuncPrefix, attr_fn_name);
  }

  // evaluate the arguments
  PL_ASSIGN_OR_RETURN(auto number_node, ProcessNumber(PYPA_PTR_CAST(Number, argument)));
  if (number_node->type() != IRNodeType::kInt) {
    return CreateAstError(arglist_parent, "Argument must be an integer for '$0.$1'",
                          kCompileTimeFuncPrefix, attr_fn_name);
  }
  int64_t time_val = static_cast<IntIR*>(number_node)->val();

  // create the ir_node;
  PL_ASSIGN_OR_RETURN(auto time_node, ir_graph_->MakeNode<IntIR>());
  std::chrono::nanoseconds time_output;
  auto time_unit = fn_type_iter->second;
  time_output = time_unit * time_val;
  PL_RETURN_IF_ERROR(time_node->Init(time_output.count(), arglist_parent));

  return time_node;
}

bool ASTWalker::IsUnitTimeFn(const std::string& fn_name) {
  return kUnitTimeFnStr.find(fn_name) != kUnitTimeFnStr.end();
}

StatusOr<IntIR*> ASTWalker::EvalCompileTimeFn(const std::string& attr_fn_name,
                                              const pypa::AstArguments& arglist,
                                              const pypa::AstPtr& arglist_parent) {
  IntIR* ir_node;
  if (attr_fn_name == "now") {
    PL_ASSIGN_OR_RETURN(ir_node, EvalCompileTimeNow(arglist, arglist_parent));
    // TODO(philkuz) (PL-445) support other funcs
  } else if (IsUnitTimeFn(attr_fn_name)) {
    PL_ASSIGN_OR_RETURN(ir_node, EvalUnitTimeFn(attr_fn_name, arglist, arglist_parent));
  } else {
    return CreateAstError(arglist_parent, "Couldn't find function with name '$0.$1'",
                          kCompileTimeFuncPrefix, attr_fn_name);
  }

  return ir_node;
}

StatusOr<ExpressionIR*> ASTWalker::ProcessDataBinOp(const pypa::AstBinOpPtr& node,
                                                    const OperatorContext& op_context) {
  std::string op_str = pypa::to_string(node->op);
  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));

  PL_ASSIGN_OR_RETURN(IRNode * left, ProcessData(node->left, op_context));
  PL_ASSIGN_OR_RETURN(IRNode * right, ProcessData(node->right, op_context));
  if (!left->IsExpression()) {
    return CreateAstError(
        node,
        "Expected left side of operation to be an expression, but got $0, which is not an "
        "expression..",
        left->type_string());
  }
  if (!right->IsExpression()) {
    return CreateAstError(
        node,
        "Expected right side of operation to be an expression, but got $0, which is not an "
        "expression.",
        right->type_string());
  }
  PL_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->MakeNode<FuncIR>());
  std::vector<ExpressionIR*> expressions = {static_cast<ExpressionIR*>(left),
                                            static_cast<ExpressionIR*>(right)};
  PL_RETURN_IF_ERROR(
      ir_node->Init(op, kCompileTimeFuncPrefix, expressions, true /* compile_time */, node));

  return ir_node;
}
StatusOr<ExpressionIR*> ASTWalker::ProcessDataCall(const pypa::AstCallPtr& node) {
  auto fn = node->function;
  if (fn->type != AstType::Attribute) {
    return CreateAstError(fn, "Expected any function calls to be made an attribute, not as a $0",
                          GetAstTypeName(fn->type));
  }
  auto fn_attr = PYPA_PTR_CAST(Attribute, fn);
  auto attr_parent = fn_attr->value;
  auto attr_fn_name = fn_attr->attribute;
  if (attr_parent->type != AstType::Name) {
    return CreateAstError(attr_parent, "Nested calls not allowed. Expected Name attr not $0",
                          GetAstTypeName(fn->type));
  }
  if (attr_fn_name->type != AstType::Name) {
    return CreateAstError(attr_fn_name, "Expected Name attr not $0", GetAstTypeName(fn->type));
  }
  // attr parent must be plc.
  auto attr_parent_str = GetNameID(attr_parent);
  if (attr_parent_str != kCompileTimeFuncPrefix) {
    return CreateAstError(attr_parent, "Namespace '$0' not found.", attr_parent_str);
  }
  auto attr_fn_str = GetNameID(attr_fn_name);
  // value must be a valid child of that namespace
  // return the corresponding value IRNode
  return EvalCompileTimeFn(attr_fn_str, node->arglist, node);
}

StatusOr<IRNode*> ASTWalker::ProcessData(const pypa::AstPtr& ast,
                                         const OperatorContext& op_context) {
  IRNode* ir_node;
  switch (ast->type) {
    case AstType::Str: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessStr(PYPA_PTR_CAST(Str, ast)));
      break;
    }
    case AstType::Number: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessNumber(PYPA_PTR_CAST(Number, ast)));
      break;
    }
    case AstType::List: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessList(PYPA_PTR_CAST(List, ast), op_context));
      break;
    }
    case AstType::Lambda: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessLambda(PYPA_PTR_CAST(Lambda, ast), op_context));
      break;
    }
    case AstType::Call: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessDataCall(PYPA_PTR_CAST(Call, ast)));
      break;
    }
    case AstType::BinOp: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessDataBinOp(PYPA_PTR_CAST(BinOp, ast), op_context));
      break;
    }
    default: {
      return CreateAstError(ast, "Couldn't find $0 in ProcessData", GetAstTypeName(ast->type));
    }
  }
  return ir_node;
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
