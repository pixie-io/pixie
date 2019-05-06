#include "src/carnot/compiler/ast_visitor.h"

namespace pl {
namespace carnot {
namespace compiler {
using pypa::AstType;
using pypa::walk_tree;

const std::unordered_map<std::string, std::string> kOP_TO_UDF_MAP = {
    {"*", "multiply"},       {"+", "add"},
    {"-", "subtract"},       {"/", "divide"},
    {">", "greaterThan"},    {"<", "lessThan"},
    {"==", "equal"},         {"!=", "notEqual"},
    {"<=", "lessThanEqual"}, {">=", "greaterThanEqual"},
    {"and", "logicalAnd"},   {"or", "logicalOr"}};
const std::unordered_map<std::string, std::chrono::nanoseconds> kUnitTimeFnStr = {
    {"minutes", std::chrono::minutes(1)},           {"hours", std::chrono::hours(1)},
    {"seconds", std::chrono::seconds(1)},           {"days", std::chrono::hours(24)},
    {"microseconds", std::chrono::microseconds(1)}, {"milliseconds", std::chrono::milliseconds(1)}};

StatusOr<std::string> ASTWalker::ExpandOpString(const std::string& op, const std::string& prefix,
                                                const pypa::AstPtr node) {
  auto op_find = kOP_TO_UDF_MAP.find(op);
  if (op_find == kOP_TO_UDF_MAP.end()) {
    return CreateAstError(absl::StrFormat("Operator '%s' not handled", op), node);
  }
  return absl::Substitute("$0.$1", prefix, op_find->second);
}

const std::unordered_map<std::string, int64_t> kTimeMapNS = {
    {"pl.second", 1e9}, {"pl.minute", 6e10}, {"pl.hour", 3.6e11}};

ASTWalker::ASTWalker(std::shared_ptr<IR> ir_graph, CompilerState* compiler_state) {
  ir_graph_ = ir_graph;
  var_table_ = VarTable();
  compiler_state_ = compiler_state;
}

Status ASTWalker::CreateAstError(const std::string& err_msg, const pypa::AstPtr& ast) {
  return error::InvalidArgument("Line $0 Col $1 : $2", ast->line, ast->column, err_msg);
}
Status ASTWalker::CreateAstError(const std::string& err_msg, const pypa::Ast& ast) {
  return CreateAstError(err_msg, std::make_shared<pypa::Ast>(ast));
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
  return absl::StrFormat("%s", type_names[static_cast<int>(type)]);
}

Status ASTWalker::ProcessExprStmtNode(const pypa::AstExpressionStatementPtr& e) {
  switch (e->expr->type) {
    case AstType::Call:
      return ProcessOpCallNode(PYPA_PTR_CAST(Call, e->expr)).status();
    default:
      return CreateAstError("Expression node not defined", e);
  }
}

Status ASTWalker::ProcessModuleNode(const pypa::AstModulePtr& m) {
  pypa::AstStmtList items_list = m->body->items;
  // iterate through all the items on this list.
  for (pypa::AstStmt stmt : items_list) {
    Status result;
    switch (stmt->type) {
      case pypa::AstType::ExpressionStatement:
        result = ProcessExprStmtNode(PYPA_PTR_CAST(ExpressionStatement, stmt));
        PL_RETURN_IF_ERROR(result);
        break;
      case pypa::AstType::Assign:
        result = ProcessAssignNode(PYPA_PTR_CAST(Assign, stmt));
        PL_RETURN_IF_ERROR(result);
        break;
      default:
        std::string err_msg =
            absl::StrFormat("Can't parse expression of type %s", GetAstTypeName(stmt->type));
        return CreateAstError(err_msg, m);
    }
  }
  return Status::OK();
}

Status ASTWalker::ProcessAssignNode(const pypa::AstAssignPtr& node) {
  // Check # nodes to assign.
  if (node->targets.size() != 1) {
    return CreateAstError("AssignNodes are only supported with one target.", node);
  }
  // Get the name that we are targeting.
  auto expr_node = node->targets[0];
  if (expr_node->type != AstType::Name) {
    return CreateAstError("Assign target must be a Name node.", expr_node);
  }
  std::string assign_name = GetNameID(expr_node);
  // Get the object that we want to assign.
  if (node->value->type != AstType::Call) {
    return CreateAstError("Assign value must be a function call.", node->value);
  }
  StatusOr<IRNode*> value = ProcessOpCallNode(PYPA_PTR_CAST(Call, node->value));

  PL_RETURN_IF_ERROR(value);

  var_table_[assign_name] = value.ValueOrDie();
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
        return CreateAstError(absl::StrFormat("Couldn't get string name out of node of type %s.",
                                              GetAstTypeName(attr->attribute->type)),
                              attr->attribute);
      }
      func_name = GetNameID(attr->attribute);
      break;
    }
    default: {
      return CreateAstError(absl::StrFormat("Couldn't get string name out of node of type %s.",
                                            GetAstTypeName(node->function->type)),
                            node->function);
    }
  }
  return func_name;
}

StatusOr<ArgMap> ASTWalker::ProcessArgs(const pypa::AstCallPtr& call_ast,
                                        const std::vector<std::string>& expected_args,
                                        bool kwargs_only) {
  return ProcessArgs(call_ast, expected_args, kwargs_only, {{}});
}
StatusOr<ArgMap> ASTWalker::ProcessArgs(
    const pypa::AstCallPtr& call_ast, const std::vector<std::string>& expected_args,
    bool kwargs_only, const std::unordered_map<std::string, IRNode*>& default_args) {
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
      // TODO(philkuz) make a string output version of CreateAstError.
      errors.push_back(CreateAstError(
          absl::Substitute("Keyword '$0' not expected in function.", key), call_ast));
      continue;
    }
    missing_or_default_args.erase(missing_or_default_args.find(key));
    PL_ASSIGN_OR_RETURN(IRNode * value, ProcessData(kw_ptr->value));
    arg_map[key] = value;
  }

  for (const auto& ma : missing_or_default_args) {
    auto find_ma = default_args.find(ma);
    if (find_ma == default_args.end()) {
      // TODO(philkuz) look for places where ast error might exit prematurely in other parts of the
      // code.
      errors.push_back(CreateAstError(
          absl::Substitute("You must set '$0' directly. No default value found.", ma), call_ast));
      continue;
    }
    arg_map[ma] = find_ma->second;
  }
  if (!errors.empty()) {
    std::vector<std::string> msg;
    for (auto const& e : errors) {
      msg.push_back(e.ToString());
    }
    return error::InvalidArgument(absl::StrJoin(msg, "\n"));
  }

  return arg_map;
}

StatusOr<IRNode*> ASTWalker::LookupName(const pypa::AstNamePtr& name_node) {
  // if doesn't exist, then
  auto find_name = var_table_.find(name_node->id);
  if (find_name == var_table_.end()) {
    std::string err_msg = absl::StrFormat("Can't find variable \"%s\".", name_node->id);
    return CreateAstError(err_msg, name_node);
  }
  IRNode* node = find_name->second;
  return node;
}

StatusOr<IRNode*> ASTWalker::ProcessOpCallNode(const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(std::string func_name, GetFuncName(node));
  IRNode* ir_node;
  if (func_name == kFromOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessOp<MemorySourceIR>(node));
  } else if (func_name == kRangeOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessRangeOp(node));
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
  } else if (func_name == kRangeAggOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessRangeAggOp(node));
  } else {
    std::string err_msg = absl::Substitute("No function named '$0'", func_name);
    return CreateAstError(err_msg, node);
  }
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessAttribute(const pypa::AstAttributePtr& node) {
  switch (node->value->type) {
    case AstType::Call: {
      return ProcessOpCallNode(PYPA_PTR_CAST(Call, node->value));
    }
    case AstType::Name: {
      return LookupName(PYPA_PTR_CAST(Name, node->value));
    }
    default: { return CreateAstError("Can't handle the attribute of this type", node->value); }
  }
}

template <typename TOpIR>
StatusOr<TOpIR*> ASTWalker::ProcessOp(const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(TOpIR * ir_node, ir_graph_->MakeNode<TOpIR>());
  if (!(ir_node->is_source()) && node->function->type != AstType::Attribute) {
    return CreateAstError(
        absl::StrFormat("Only source operators can be called from outside a table reference. '%s' "
                        "needs to be called as an attribute of a table.",
                        ir_node->type_string()),
        node->function);
  }
  IRNode* call_result = nullptr;
  if (node->function->type == AstType::Attribute) {
    PL_ASSIGN_OR_RETURN(call_result, ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  }
  PL_ASSIGN_OR_RETURN(ArgMap args,
                      ProcessArgs(node, ir_node->ArgKeys(), true, ir_node->DefaultArgValues(node)));

  PL_RETURN_IF_ERROR(ir_node->Init(call_result, args, node));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessRangeOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(absl::StrFormat("Expected Range to be an attribute, not a %s",
                                          GetAstTypeName(node->function->type)),
                          node->function);
  }
  PL_ASSIGN_OR_RETURN(RangeIR * ir_node, ir_graph_->MakeNode<RangeIR>());
  // Setup the default arguments.
  PL_ASSIGN_OR_RETURN(IntIR * default_stop_node, MakeTimeNow(node));

  PL_ASSIGN_OR_RETURN(ArgMap args,
                      ProcessArgs(node, {"start", "stop"}, true, {{"stop", default_stop_node}}));
  PL_ASSIGN_OR_RETURN(IRNode * call_result,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  PL_RETURN_IF_ERROR(ir_node->Init(call_result, args["start"], args["stop"], node));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessRangeAggOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(absl::StrFormat("Expected RangeAgg to be an attribute, not a %s",
                                          GetAstTypeName(node->function->type)),
                          node->function);
  }

  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, {"fn", "by", "size"}, true));
  PL_ASSIGN_OR_RETURN(IRNode * call_result,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));

  // Create Map IR.
  PL_ASSIGN_OR_RETURN(MapIR * map_ir_node, ir_graph_->MakeNode<MapIR>());

  // pl.mod(by_col, size).
  DCHECK(args["by"]->type() == IRNodeType::LambdaType);
  auto by_col_ir_node = static_cast<LambdaIR*>(args["by"])->col_exprs()[0].node;
  PL_ASSIGN_OR_RETURN(FuncIR * mod_ir_node, ir_graph_->MakeNode<FuncIR>());
  PL_RETURN_IF_ERROR(
      mod_ir_node->Init("pl.modulo", std::vector<IRNode*>({by_col_ir_node, args["size"]}), node));

  // pl.subtract(by_col, pl.mod(by_col, size)).
  PL_ASSIGN_OR_RETURN(FuncIR * sub_ir_node, ir_graph_->MakeNode<FuncIR>());
  PL_RETURN_IF_ERROR(
      sub_ir_node->Init("pl.subtract", std::vector<IRNode*>({by_col_ir_node, mod_ir_node}), node));

  // Map(lambda r: {'group': pl.subtract(by_col, pl.modulo(by_col, size))}.
  PL_ASSIGN_OR_RETURN(LambdaIR * map_lambda_ir_node, ir_graph_->MakeNode<LambdaIR>());
  // Pull in all columns needed in fn.
  ColExpressionVector map_exprs = ColExpressionVector({ColumnExpression{"group", sub_ir_node}});
  for (const auto& name : static_cast<LambdaIR*>(args["fn"])->expected_column_names()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col_node, ir_graph_->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(col_node->Init(name, node));
    map_exprs.push_back(ColumnExpression{name, col_node});
  }
  PL_RETURN_IF_ERROR(map_lambda_ir_node->Init(
      std::unordered_set<std::string>({static_cast<ColumnIR*>(by_col_ir_node)->col_name()}),
      map_exprs, node));
  ArgMap map_args{{"fn", map_lambda_ir_node}};
  PL_RETURN_IF_ERROR(map_ir_node->Init(call_result, map_args, node));

  // Create BlockingAggIR.
  PL_ASSIGN_OR_RETURN(BlockingAggIR * agg_ir_node, ir_graph_->MakeNode<BlockingAggIR>());

  // by = lambda r: r.group.
  PL_ASSIGN_OR_RETURN(ColumnIR * agg_col_ir_node, ir_graph_->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(agg_col_ir_node->Init("group", node));
  PL_ASSIGN_OR_RETURN(LambdaIR * agg_by_ir_node, ir_graph_->MakeNode<LambdaIR>());
  PL_RETURN_IF_ERROR(
      agg_by_ir_node->Init(std::unordered_set<std::string>({"group"}), agg_col_ir_node, node));

  // Agg(fn = fn, by = lambda r: r.group).
  ArgMap agg_args{{"by", agg_by_ir_node}, {"fn", args["fn"]}};
  PL_RETURN_IF_ERROR(agg_ir_node->Init(map_ir_node, agg_args, node));
  return agg_ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessStr(const pypa::AstStrPtr& ast) {
  PL_ASSIGN_OR_RETURN(StringIR * ir_node, ir_graph_->MakeNode<StringIR>());
  PL_ASSIGN_OR_RETURN(auto str_value, GetStrAstValue(ast));
  PL_RETURN_IF_ERROR(ir_node->Init(str_value, ast));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessList(const pypa::AstListPtr& ast) {
  ListIR* ir_node = ir_graph_->MakeNode<ListIR>().ValueOrDie();
  std::vector<IRNode*> children;
  for (auto& child : ast->elements) {
    PL_ASSIGN_OR_RETURN(IRNode * child_node, ProcessData(child));
    children.push_back(child_node);
  }
  PL_RETURN_IF_ERROR(ir_node->Init(ast, children));
  return ir_node;
}
StatusOr<LambdaExprReturn> ASTWalker::LookupPLTimeAttribute(const std::string& attribute_name,
                                                            const pypa::AstPtr& parent_node) {
  auto time_idx = kTimeMapNS.find(attribute_name);
  if (time_idx == kTimeMapNS.end()) {
    return CreateAstError(absl::Substitute("Couldn't find attribute $0", attribute_name),
                          parent_node);
  }
  PL_ASSIGN_OR_RETURN(TimeIR * time_node, ir_graph_->MakeNode<TimeIR>());
  PL_RETURN_IF_ERROR(time_node->Init(time_idx->second, parent_node));
  return LambdaExprReturn(time_node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaAttribute(const std::string& arg_name,
                                                             const pypa::AstAttributePtr& node) {
  // make sure that the attribute values are of type name.
  if (node->attribute->type != AstType::Name) {
    return CreateAstError(absl::StrFormat("Attribute must be a variable, not a %s",
                                          GetAstTypeName(node->attribute->type)),
                          node->attribute);
  }
  if (node->value->type != AstType::Name) {
    return CreateAstError(absl::StrFormat("Attribute value must be a variable, not a %s",
                                          GetAstTypeName(node->value->type)),
                          node->value);
  }
  auto value = GetNameID(node->value);
  auto attribute = GetNameID(node->attribute);

  // if the value is equal to the arg_name, then the attribute is a column
  if (value == arg_name) {
    std::unordered_set<std::string> column_names;
    column_names.insert(attribute);
    PL_ASSIGN_OR_RETURN(ColumnIR * expr, ir_graph_->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(expr->Init(attribute, node));
    return LambdaExprReturn(expr, column_names);
  }
  if (value == kUDFPrefix) {
    return LambdaExprReturn(absl::StrFormat("%s.%s", value, attribute), true /*is_pixie_attr*/);
  }
  return CreateAstError(absl::StrFormat("Couldn't find value %s", value), node);
}

StatusOr<IRNode*> ASTWalker::ProcessNumber(const pypa::AstNumberPtr& node) {
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
      return CreateAstError(absl::StrFormat("Couldn't find number type %d", node->num_type), node);
  }
}

StatusOr<LambdaExprReturn> ASTWalker::BuildLambdaFunc(
    const std::string& fn_name, const std::vector<LambdaExprReturn>& children_ret_expr,
    const pypa::AstPtr& parent_node) {
  PL_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->MakeNode<FuncIR>());
  std::vector<IRNode*> expressions;
  auto ret = LambdaExprReturn(ir_node);
  for (auto expr_ret : children_ret_expr) {
    if (expr_ret.StringOnly()) {
      PL_ASSIGN_OR_RETURN(auto attr_expr, LookupPLTimeAttribute(expr_ret.str_, parent_node));
      expressions.push_back(attr_expr.expr_);
    } else {
      expressions.push_back(expr_ret.expr_);
      ret.MergeColumns(expr_ret);
    }
  }
  PL_RETURN_IF_ERROR(ir_node->Init(fn_name, expressions, parent_node));
  return ret;
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaBinOp(const std::string& arg_name,
                                                         const pypa::AstBinOpPtr& node) {
  std::string op_str = pypa::to_string(node->op);
  PL_ASSIGN_OR_RETURN(std::string fn_name, ExpandOpString(op_str, kUDFPrefix, node));
  std::vector<LambdaExprReturn> children_ret_expr;
  PL_ASSIGN_OR_RETURN(auto left_expr_ret, ProcessLambdaExpr(arg_name, node->left));
  PL_ASSIGN_OR_RETURN(auto right_expr_ret, ProcessLambdaExpr(arg_name, node->right));
  children_ret_expr.push_back(left_expr_ret);
  children_ret_expr.push_back(right_expr_ret);
  return BuildLambdaFunc(fn_name, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaBoolOp(const std::string& arg_name,
                                                          const pypa::AstBoolOpPtr& node) {
  std::string op_str = pypa::to_string(node->op);
  PL_ASSIGN_OR_RETURN(std::string fn_name, ExpandOpString(op_str, kUDFPrefix, node));
  std::vector<LambdaExprReturn> children_ret_expr;
  for (auto comp : node->values) {
    PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_name, comp));
    children_ret_expr.push_back(rt);
  }
  return BuildLambdaFunc(fn_name, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaCompare(const std::string& arg_name,
                                                           const pypa::AstComparePtr& node) {
  DCHECK_EQ(node->operators.size(), 1ULL);
  std::string op_str = pypa::to_string(node->operators[0]);
  PL_ASSIGN_OR_RETURN(std::string fn_name, ExpandOpString(op_str, kUDFPrefix, node));
  std::vector<LambdaExprReturn> children_ret_expr;
  if (node->comparators.size() != 1) {
    return CreateAstError(
        absl::StrFormat("Only expected one argument to the right of '%s'.", op_str), node);
  }
  PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_name, node->left));
  children_ret_expr.push_back(rt);
  for (auto comp : node->comparators) {
    PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_name, comp));
    children_ret_expr.push_back(rt);
  }
  return BuildLambdaFunc(fn_name, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaCall(const std::string& arg_name,
                                                        const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(auto attr_result, ProcessLambdaExpr(arg_name, node->function));
  if (!attr_result.StringOnly()) {
    return CreateAstError("Expected a string for the return", node);
  }
  auto arglist = node->arglist;
  if (!arglist.defaults.empty() || !arglist.keywords.empty()) {
    return CreateAstError("Only non-default and non-keyword args allowed.", node);
  }

  std::string fn_name = attr_result.str_;
  std::vector<LambdaExprReturn> children_ret_expr;
  for (auto arg_ast : arglist.arguments) {
    PL_ASSIGN_OR_RETURN(auto rt, ProcessLambdaExpr(arg_name, arg_ast));
    children_ret_expr.push_back(rt);
  }

  return BuildLambdaFunc(fn_name, children_ret_expr, node);
}

/**
 * @brief Wraps an IrNode StatusOr return with the LambdaExprReturn StatusOr
 *
 * @param node
 * @return StatusOr<LambdaExprReturn>
 */
StatusOr<LambdaExprReturn> WrapLambdaExprReturn(StatusOr<IRNode*> node) {
  PL_RETURN_IF_ERROR(node);
  return LambdaExprReturn(node.ValueOrDie());
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaList(const std::string& arg_name,
                                                        const pypa::AstListPtr& node) {
  ListIR* ir_node = ir_graph_->MakeNode<ListIR>().ValueOrDie();
  LambdaExprReturn expr_return(ir_node);
  std::vector<IRNode*> children;
  for (auto& child : node->elements) {
    PL_ASSIGN_OR_RETURN(auto child_attr, ProcessLambdaExpr(arg_name, child));
    if (child_attr.StringOnly() || child_attr.expr_->type() != IRNodeType::ColumnType) {
      return CreateAstError("Expect Lambda list to only contain column names.", node);
    }
    expr_return.MergeColumns(child_attr);
    children.push_back(child_attr.expr_);
  }
  PL_RETURN_IF_ERROR(ir_node->Init(node, children));
  return expr_return;
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaExpr(const std::string& arg_name,
                                                        const pypa::AstPtr& node) {
  LambdaExprReturn expr_return;
  switch (node->type) {
    case AstType::BinOp: {
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaBinOp(arg_name, PYPA_PTR_CAST(BinOp, node)));
      break;
    }
    case AstType::Attribute: {
      PL_ASSIGN_OR_RETURN(expr_return,
                          ProcessLambdaAttribute(arg_name, PYPA_PTR_CAST(Attribute, node)));
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
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaCall(arg_name, PYPA_PTR_CAST(Call, node)));
      break;
    }
    case AstType::List: {
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaList(arg_name, PYPA_PTR_CAST(List, node)));
      break;
    }
    case AstType::Compare: {
      PL_ASSIGN_OR_RETURN(expr_return,
                          ProcessLambdaCompare(arg_name, PYPA_PTR_CAST(Compare, node)));
      break;
    }
    case AstType::BoolOp: {
      PL_ASSIGN_OR_RETURN(expr_return, ProcessLambdaBoolOp(arg_name, PYPA_PTR_CAST(BoolOp, node)));
      break;
    }
    default: {
      return CreateAstError(
          absl::StrFormat("Node of type %s not allowed for expression in Lambda function.",
                          GetAstTypeName(node->type)),
          node);
    }
  }
  return expr_return;
}

StatusOr<std::string> ASTWalker::ProcessLambdaArgs(const pypa::AstLambdaPtr& node) {
  auto arg_ast = node->arguments;
  if (arg_ast.arguments.size() != 1) {
    return CreateAstError("Only allow 1 arg for the lambda.", node);
  }
  if (!arg_ast.defaults.empty() && arg_ast.defaults[0]) {
    return CreateAstError(
        absl::StrFormat("No default arguments allowed for lambdas. Found %d default args.",
                        arg_ast.defaults.size()),
        node);
  }
  if (!arg_ast.keywords.empty()) {
    return CreateAstError("No keyword arguments allowed for lambdas.", node);
  }
  auto arg_node = arg_ast.arguments[0];
  if (arg_node->type != AstType::Name) {
    return CreateAstError("Argument must be a Name.", node);
  }
  return GetNameID(arg_node);
}

StatusOr<LambdaBodyReturn> ASTWalker::ProcessLambdaDict(const std::string& arg_name,
                                                        const pypa::AstDictPtr& body_dict) {
  auto return_val = LambdaBodyReturn();
  for (size_t i = 0; i < body_dict->keys.size(); i++) {
    auto key_str_ast = body_dict->keys[i];
    auto value_ast = body_dict->values[i];
    PL_ASSIGN_OR_RETURN(auto key_string, GetStrAstValue(key_str_ast));
    PL_ASSIGN_OR_RETURN(auto expr_ret, ProcessLambdaExpr(arg_name, value_ast));
    if (expr_ret.is_pixie_attr_) {
      PL_ASSIGN_OR_RETURN(expr_ret, BuildLambdaFunc(expr_ret.str_, {}, body_dict));
    }
    PL_RETURN_IF_ERROR(return_val.AddExprResult(key_string, expr_ret));
  }
  return return_val;
}

StatusOr<IRNode*> ASTWalker::ProcessLambda(const pypa::AstLambdaPtr& ast) {
  LambdaIR* ir_node = ir_graph_->MakeNode<LambdaIR>().ValueOrDie();
  PL_ASSIGN_OR_RETURN(std::string arg_name, ProcessLambdaArgs(ast));
  LambdaBodyReturn return_struct;
  switch (ast->body->type) {
    case AstType::Dict: {
      PL_ASSIGN_OR_RETURN(return_struct,
                          ProcessLambdaDict(arg_name, PYPA_PTR_CAST(Dict, ast->body)));
      PL_RETURN_IF_ERROR(ir_node->Init(return_struct.input_relation_columns_,
                                       return_struct.col_exprs_, ast->body));
      return ir_node;
    }

    default: {
      PL_ASSIGN_OR_RETURN(LambdaExprReturn return_val, ProcessLambdaExpr(arg_name, ast->body));
      PL_RETURN_IF_ERROR(
          ir_node->Init(return_val.input_relation_columns_, return_val.expr_, ast->body));
      return ir_node;
    }
  }
}

StatusOr<IntIR*> ASTWalker::MakeTimeNow(const pypa::AstPtr& ast_node) {
  PL_ASSIGN_OR_RETURN(IntIR * ir_node, ir_graph_->MakeNode<IntIR>());
  PL_RETURN_IF_ERROR(ir_node->Init(compiler_state_->time_now().val, ast_node));
  return ir_node;
}

StatusOr<IntIR*> ASTWalker::EvalCompileTimeNow(const pypa::AstArguments& arglist) {
  if (!arglist.arguments.empty() || !arglist.keywords.empty()) {
    return CreateAstError(
        absl::Substitute("No Arguments expected for '$0.$1' fn", kCompileTimePrefix, "now"),
        arglist);
  }
  PL_ASSIGN_OR_RETURN(IntIR * ir_node, MakeTimeNow(std::make_shared<pypa::Ast>(arglist)));
  return ir_node;
}

StatusOr<IntIR*> ASTWalker::EvalUnitTimeFn(const std::string& attr_fn_name,
                                           const pypa::AstArguments& arglist) {
  auto fn_type_iter = kUnitTimeFnStr.find(attr_fn_name);
  if (fn_type_iter == kUnitTimeFnStr.end()) {
    return CreateAstError(
        absl::Substitute("Function '$0.$1' not found", kCompileTimePrefix, attr_fn_name), arglist);
  }
  if (!arglist.keywords.empty()) {
    return CreateAstError(absl::Substitute("No Keyword args expected for '$0.$1' fn",
                                           kCompileTimePrefix, attr_fn_name),
                          arglist);
  }
  if (arglist.arguments.size() != 1) {
    return CreateAstError(
        absl::Substitute("Only expected one arg for '$0.$1' fn", kCompileTimePrefix, attr_fn_name),
        arglist);
  }
  auto argument = arglist.arguments[0];
  if (argument->type != AstType::Number) {
    return CreateAstError(absl::Substitute("Argument must be an integer for '$0.$1'",
                                           kCompileTimePrefix, attr_fn_name),
                          arglist);
  }

  // evaluate the arguments
  PL_ASSIGN_OR_RETURN(auto number_node, ProcessNumber(PYPA_PTR_CAST(Number, argument)));
  if (number_node->type() != IRNodeType::IntType) {
    return CreateAstError(absl::Substitute("Argument must be an integer for '$0.$1'",
                                           kCompileTimePrefix, attr_fn_name),
                          arglist);
  }
  int64_t time_val = static_cast<IntIR*>(number_node)->val();

  // create the ir_node;
  PL_ASSIGN_OR_RETURN(auto time_node, ir_graph_->MakeNode<IntIR>());
  std::chrono::nanoseconds time_output;
  auto time_unit = fn_type_iter->second;
  time_output = time_unit * time_val;
  PL_RETURN_IF_ERROR(time_node->Init(time_output.count(), std::make_shared<pypa::Ast>(arglist)));

  return time_node;
}

bool ASTWalker::IsUnitTimeFn(const std::string& fn_name) {
  return kUnitTimeFnStr.find(fn_name) != kUnitTimeFnStr.end();
}

StatusOr<IntIR*> ASTWalker::EvalCompileTimeFn(const std::string& attr_fn_name,
                                              const pypa::AstArguments& arglist) {
  IntIR* ir_node;
  if (attr_fn_name == "now") {
    PL_ASSIGN_OR_RETURN(ir_node, EvalCompileTimeNow(arglist));
    // TODO(philkuz) (PL-445) support other funcs
  } else if (IsUnitTimeFn(attr_fn_name)) {
    PL_ASSIGN_OR_RETURN(ir_node, EvalUnitTimeFn(attr_fn_name, arglist));
  } else {
    return CreateAstError(absl::Substitute("Couldn't find function with name '$0.$1'",
                                           kCompileTimePrefix, attr_fn_name),
                          arglist);
  }

  return ir_node;
}

StatusOr<IRNode*> ASTWalker::WrapAstError(StatusOr<IRNode*> status_or,
                                          const pypa::AstPtr& parent_node) {
  if (status_or.ok()) {
    auto val = status_or.ValueOrDie();
    return val;
  }
  return CreateAstError(status_or.msg(), parent_node);
}

StatusOr<IRNode*> ASTWalker::ProcessDataBinOp(const pypa::AstBinOpPtr& node) {
  std::string op_str = pypa::to_string(node->op);
  PL_ASSIGN_OR_RETURN(std::string fn_name, ExpandOpString(op_str, kCompileTimePrefix, node));

  PL_ASSIGN_OR_RETURN(IRNode * left, ProcessData(node->left));
  PL_ASSIGN_OR_RETURN(IRNode * right, ProcessData(node->right));
  PL_ASSIGN_OR_RETURN(FuncIR * ir_node, ir_graph_->MakeNode<FuncIR>());
  std::vector<IRNode*> expressions = {left, right};
  PL_RETURN_IF_ERROR(ir_node->Init(fn_name, expressions, node));

  return ir_node;
}
StatusOr<IRNode*> ASTWalker::ProcessDataCall(const pypa::AstCallPtr& node) {
  auto fn = node->function;
  if (fn->type != AstType::Attribute) {
    return CreateAstError(
        absl::StrFormat("Expected any function calls to be made an attribute, not as a %s",
                        GetAstTypeName(fn->type)),
        fn);
  }
  auto fn_attr = PYPA_PTR_CAST(Attribute, fn);
  auto attr_parent = fn_attr->value;
  auto attr_fn_name = fn_attr->attribute;
  if (attr_parent->type != AstType::Name) {
    return CreateAstError(absl::StrFormat("Nested calls not allowed. Expected Name attr not %s",
                                          GetAstTypeName(fn->type)),
                          attr_parent);
  }
  if (attr_fn_name->type != AstType::Name) {
    return CreateAstError(absl::StrFormat("Expected Name attr not %s", GetAstTypeName(fn->type)),
                          attr_fn_name);
  }
  // attr parent must be plc.
  auto attr_parent_str = GetNameID(attr_parent);
  if (attr_parent_str != kCompileTimePrefix) {
    return CreateAstError(absl::StrFormat("Namespace '%s' not found.", attr_parent_str),
                          attr_parent);
  }
  auto attr_fn_str = GetNameID(attr_fn_name);
  // value must be a valid child of that namespace
  // return the corresponding value IRNode
  return WrapAstError(EvalCompileTimeFn(attr_fn_str, node->arglist), node);
}
StatusOr<IRNode*> ASTWalker::MakeDefaultAggByArg(const pypa::AstPtr& ast) {
  PL_ASSIGN_OR_RETURN(auto node, ir_graph_->MakeNode<BoolIR>());
  PL_RETURN_IF_ERROR(node->Init(true, ast));
  return node;
}
// TODO(philkuz) (PL-402) remove this and allow for optional kwargs in the ProcessArgs function.
StatusOr<IRNode*> ASTWalker::ProcessNameData(const pypa::AstNamePtr& ast) {
  auto name_str = ast->id;
  if (name_str != "None") {
    return CreateAstError(absl::StrFormat("'%s' is not a variable or a keyword.", name_str), ast);
  }
  PL_ASSIGN_OR_RETURN(auto ir_node, BlockingAggIR::MakeDefaultAggByArg(ir_graph_.get(), ast));
  return ir_node;
}
StatusOr<IRNode*> ASTWalker::ProcessData(const pypa::AstPtr& ast) {
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
      PL_ASSIGN_OR_RETURN(ir_node, ProcessList(PYPA_PTR_CAST(List, ast)));
      break;
    }
    case AstType::Lambda: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessLambda(PYPA_PTR_CAST(Lambda, ast)));
      break;
    }
    case AstType::Name: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessNameData(PYPA_PTR_CAST(Name, ast)));
      break;
    }
    case AstType::Call: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessDataCall(PYPA_PTR_CAST(Call, ast)));
      break;
    }
    case AstType::BinOp: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessDataBinOp(PYPA_PTR_CAST(BinOp, ast)));
      break;
    }
    default: {
      std::string err_msg =
          absl::StrFormat("Couldn't find %s in ProcessData", GetAstTypeName(ast->type));
      return CreateAstError(err_msg, ast);
    }
  }
  return ir_node;
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
