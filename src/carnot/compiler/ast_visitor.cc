#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {
using pypa::AstType;
using pypa::walk_tree;

const std::unordered_map<std::string, std::string> kOP_TO_UDF_MAP = {
    {"*", "pl.multiply"}, {"+", "pl.add"}, {"-", "pl.subtract"}, {"/", "pl.divide"}};

const std::unordered_map<std::string, int64_t> kTimeMapNS = {
    {"pl.second", 1e9}, {"pl.minute", 6e10}, {"pl.hour", 3.6e11}};

ASTWalker::ASTWalker(std::shared_ptr<IR> ir_graph) {
  ir_graph_ = ir_graph;
  var_table_ = VarTable();
}

Status ASTWalker::CreateAstError(const std::string& err_msg, const pypa::AstPtr& ast) {
  return error::InvalidArgument("Line $0 Col $1 : $2", ast->line, ast->column, err_msg);
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
  auto arg_ast = call_ast->arglist;
  if (!kwargs_only) {
    return error::Unimplemented("Only supporting kwargs for now.");
  }
  ArgMap arg_map;
  // Set to keep track of args that are not yet found.
  std::unordered_set<std::string> missing_args;
  missing_args.insert(expected_args.begin(), expected_args.end());

  // Iterate through the keywords
  for (auto& k : arg_ast.keywords) {
    pypa::AstKeywordPtr kw_ptr = PYPA_PTR_CAST(Keyword, k);
    std::string key = GetNameID(kw_ptr->name);
    if (missing_args.find(key) == missing_args.end()) {
      return CreateAstError(absl::Substitute("Keyword '$0' not expected in function.", key),
                            call_ast);
    }
    missing_args.erase(missing_args.find(key));
    PL_ASSIGN_OR_RETURN(IRNode * value, ProcessData(kw_ptr->value));
    arg_map[key] = value;
  }
  if (missing_args.size() > 0) {
    return CreateAstError(
        absl::Substitute("Didn't find keywords '[$0]' in function. Please add them.",
                         absl::StrJoin(missing_args, ",")),
        call_ast);
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
    PL_ASSIGN_OR_RETURN(ir_node, ProcessFromOp(node));
  } else if (func_name == kRangeOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessRangeOp(node));
  } else if (func_name == kMapOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessMapOp(node));
  } else if (func_name == kAggOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessAggOp(node));
  } else if (func_name == kSinkOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessSinkOp(node));
  } else if (func_name == kRangeAggOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessRangeAggOp(node));
  } else {
    std::string err_msg = absl::Substitute("No function named '$0'", func_name);
    return CreateAstError(err_msg, node);
  }
  ir_node->SetLineCol(node->line, node->column);
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessFromOp(const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(MemorySourceIR * ir_node, ir_graph_->MakeNode<MemorySourceIR>());
  // Get the arguments in the node.
  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, {"table", "select"}, true));
  PL_RETURN_IF_ERROR(ir_node->Init(args["table"], args["select"]));
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

StatusOr<IRNode*> ASTWalker::ProcessSinkOp(const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(MemorySinkIR * ir_node, ir_graph_->MakeNode<MemorySinkIR>());

  PL_ASSIGN_OR_RETURN(IRNode * call_result,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, {"name"}, true));
  PL_ASSIGN_OR_RETURN(std::string name_str, IRUtils::GetStrIRValue(*args["name"]));
  PL_RETURN_IF_ERROR(ir_node->Init(call_result, name_str));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessRangeOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(absl::StrFormat("Expected Range to be an attribute, not a %s",
                                          GetAstTypeName(node->function->type)),
                          node->function);
  }
  PL_ASSIGN_OR_RETURN(RangeIR * ir_node, ir_graph_->MakeNode<RangeIR>());
  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, {"time"}, true));
  PL_ASSIGN_OR_RETURN(IRNode * call_result,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  PL_RETURN_IF_ERROR(ir_node->Init(call_result, args["time"]));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessMapOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(absl::StrFormat("Expected Map to be an attribute, not a %s",
                                          GetAstTypeName(node->function->type)),
                          node->function);
  }
  PL_ASSIGN_OR_RETURN(MapIR * ir_node, ir_graph_->MakeNode<MapIR>());
  // Get arguments.
  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, {"fn"}, true));
  PL_ASSIGN_OR_RETURN(IRNode * call_result,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));
  Status status = ir_node->Init(call_result, args["fn"]);
  if (status.ok()) {
    return ir_node;
  } else {
    return status;
  }
}

StatusOr<IRNode*> ASTWalker::ProcessAggOp(const pypa::AstCallPtr& node) {
  if (node->function->type != AstType::Attribute) {
    return CreateAstError(absl::StrFormat("Expected Agg to be an attribute, not a %s",
                                          GetAstTypeName(node->function->type)),
                          node->function);
  }
  PL_ASSIGN_OR_RETURN(AggIR * ir_node, ir_graph_->MakeNode<AggIR>());
  // Get arguments.
  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, {"by", "fn"}, true));
  PL_ASSIGN_OR_RETURN(IRNode * call_result,
                      ProcessAttribute(PYPA_PTR_CAST(Attribute, node->function)));

  PL_RETURN_IF_ERROR(ir_node->Init(call_result, args["by"], args["fn"]));
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
      mod_ir_node->Init("pl.modulo", std::vector<IRNode*>({by_col_ir_node, args["size"]})));

  // pl.subtract(by_col, pl.mod(by_col, size)).
  PL_ASSIGN_OR_RETURN(FuncIR * sub_ir_node, ir_graph_->MakeNode<FuncIR>());
  PL_RETURN_IF_ERROR(
      sub_ir_node->Init("pl.subtract", std::vector<IRNode*>({by_col_ir_node, mod_ir_node})));

  // Map(lambda r: {'group': pl.subtract(by_col, pl.modulo(by_col, size))}.
  PL_ASSIGN_OR_RETURN(LambdaIR * map_lambda_ir_node, ir_graph_->MakeNode<LambdaIR>());
  // Pull in all columns needed in fn.
  ColExpressionVector map_exprs = ColExpressionVector({ColumnExpression{"group", sub_ir_node}});
  for (const auto& name : static_cast<LambdaIR*>(args["fn"])->expected_column_names()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col_node, ir_graph_->MakeNode<ColumnIR>());
    PL_RETURN_IF_ERROR(col_node->Init(name));
    map_exprs.push_back(ColumnExpression{name, col_node});
  }
  PL_RETURN_IF_ERROR(map_lambda_ir_node->Init(
      std::unordered_set<std::string>({static_cast<ColumnIR*>(by_col_ir_node)->col_name()}),
      map_exprs));
  PL_RETURN_IF_ERROR(map_ir_node->Init(call_result, map_lambda_ir_node));

  // Create AggIR.
  PL_ASSIGN_OR_RETURN(AggIR * agg_ir_node, ir_graph_->MakeNode<AggIR>());

  // by = lambda r: r.group.
  PL_ASSIGN_OR_RETURN(ColumnIR * agg_col_ir_node, ir_graph_->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(agg_col_ir_node->Init("group"));
  PL_ASSIGN_OR_RETURN(LambdaIR * agg_by_ir_node, ir_graph_->MakeNode<LambdaIR>());
  PL_RETURN_IF_ERROR(
      agg_by_ir_node->Init(std::unordered_set<std::string>({"group"}), agg_col_ir_node));

  // Agg(fn = fn, by = lambda r: r.group).
  PL_RETURN_IF_ERROR(agg_ir_node->Init(map_ir_node, agg_by_ir_node, args["fn"]));
  return agg_ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessStr(const pypa::AstStrPtr& ast) {
  PL_ASSIGN_OR_RETURN(StringIR * ir_node, ir_graph_->MakeNode<StringIR>());
  PL_ASSIGN_OR_RETURN(auto str_value, GetStrAstValue(ast));
  PL_RETURN_IF_ERROR(ir_node->Init(str_value));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessList(const pypa::AstListPtr& ast) {
  ListIR* ir_node = ir_graph_->MakeNode<ListIR>().ValueOrDie();
  for (auto& child : ast->elements) {
    PL_ASSIGN_OR_RETURN(IRNode * child_node, ProcessData(child));
    PL_RETURN_IF_ERROR(ir_node->AddListItem(child_node));
  }
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
  PL_RETURN_IF_ERROR(time_node->Init(time_idx->second));
  time_node->SetLineCol(parent_node->line, parent_node->column);
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
    PL_RETURN_IF_ERROR(expr->Init(attribute));
    return LambdaExprReturn(expr, column_names);
  } else if (value == kUDFPrefix) {
    return LambdaExprReturn(absl::StrFormat("%s.%s", value, attribute));
  }
  return CreateAstError(absl::StrFormat("Couldn't find value %s", value), node);
}

StatusOr<IRNode*> ASTWalker::ProcessNumber(const pypa::AstNumberPtr& node) {
  switch (node->num_type) {
    case pypa::AstNumber::Type::Float: {
      PL_ASSIGN_OR_RETURN(FloatIR * ir_node, ir_graph_->MakeNode<FloatIR>());
      PL_RETURN_IF_ERROR(ir_node->Init(node->floating));
      return ir_node;
    }
    case pypa::AstNumber::Type::Integer:
    case pypa::AstNumber::Type::Long: {
      PL_ASSIGN_OR_RETURN(IntIR * ir_node, ir_graph_->MakeNode<IntIR>());
      PL_RETURN_IF_ERROR(ir_node->Init(node->integer));
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
  PL_RETURN_IF_ERROR(ir_node->Init(fn_name, expressions));
  return ret;
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaBinOp(const std::string& arg_name,
                                                         const pypa::AstBinOpPtr& node) {
  std::string op_str = pypa::to_string(node->op);
  // map the operator to a string
  auto op_find = kOP_TO_UDF_MAP.find(op_str);
  if (op_find == kOP_TO_UDF_MAP.end()) {
    return CreateAstError(absl::StrFormat("Operator '%s' not handled", op_str), node);
  }

  std::string fn_name = op_find->second;
  std::vector<LambdaExprReturn> children_ret_expr;
  PL_ASSIGN_OR_RETURN(auto left_expr_ret, ProcessLambdaExpr(arg_name, node->left));
  PL_ASSIGN_OR_RETURN(auto right_expr_ret, ProcessLambdaExpr(arg_name, node->right));
  children_ret_expr.push_back(left_expr_ret);
  children_ret_expr.push_back(right_expr_ret);
  return BuildLambdaFunc(fn_name, children_ret_expr, node);
}

StatusOr<LambdaExprReturn> ASTWalker::ProcessLambdaCall(const std::string& arg_name,
                                                        const pypa::AstCallPtr& node) {
  PL_ASSIGN_OR_RETURN(auto attr_result, ProcessLambdaExpr(arg_name, node->function));
  if (!attr_result.StringOnly()) {
    return CreateAstError("Expected a string for the return", node);
  }
  auto arglist = node->arglist;
  if (arglist.defaults.size() != 0 || arglist.keywords.size() != 0) {
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
  for (auto& child : node->elements) {
    if (child->type != AstType::Attribute) {
      return CreateAstError(
          absl::StrFormat("Expect Lambda list to only contain column names, not %s",
                          GetAstTypeName(node->type)),
          node);
    }
    PL_ASSIGN_OR_RETURN(auto child_attr,
                        ProcessLambdaAttribute(arg_name, PYPA_PTR_CAST(Attribute, child)));
    if (child_attr.StringOnly()) {
      return CreateAstError("Expect Lambda list to only contain column names.", node);
    }
    expr_return.MergeColumns(child_attr);
    PL_RETURN_IF_ERROR(ir_node->AddListItem(child_attr.expr_));
  }
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
    default: {
      return CreateAstError(
          absl::StrFormat("Node of type %s not allowed for expression in Lambda function.",
                          GetAstTypeName(node->type)),
          node);
    }
  }
  if (!expr_return.StringOnly()) {
    expr_return.expr_->SetLineCol(node->line, node->column);
  }
  return expr_return;
}

StatusOr<std::string> ASTWalker::ProcessLambdaArgs(const pypa::AstLambdaPtr& node) {
  auto arg_ast = node->arguments;
  if (arg_ast.arguments.size() != 1) {
    return CreateAstError("Only allow 1 arg for the lambda.", node);
  }
  if (arg_ast.defaults.size() != 0 && arg_ast.defaults[0]) {
    return CreateAstError(
        absl::StrFormat("No default arguments allowed for lambdas. Found %d default args.",
                        arg_ast.defaults.size()),
        node);
  }
  if (arg_ast.keywords.size() != 0) {
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
    PL_ASSIGN_OR_RETURN(auto key_string, GetStrAstValue(key_str_ast));
    PL_ASSIGN_OR_RETURN(auto expr_ret, ProcessLambdaExpr(arg_name, body_dict->values[i]));

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
      PL_RETURN_IF_ERROR(
          ir_node->Init(return_struct.input_relation_columns_, return_struct.col_exprs_));
      return ir_node;
    }

    default: {
      PL_ASSIGN_OR_RETURN(LambdaExprReturn return_val, ProcessLambdaExpr(arg_name, ast->body));
      PL_RETURN_IF_ERROR(ir_node->Init(return_val.input_relation_columns_, return_val.expr_));
      return ir_node;
    }
  }
}

// TODO(philkuz) (PL-402) remove this and allow for optional kwargs in the ProcessArgs function.
StatusOr<IRNode*> ASTWalker::ProcessNameData(const pypa::AstNamePtr& ast) {
  auto name_str = ast->id;
  if (name_str != "None") {
    return CreateAstError(absl::StrFormat("Couldn't process '%s'.", name_str), ast);
  }
  PL_ASSIGN_OR_RETURN(auto node, ir_graph_->MakeNode<BoolIR>());
  PL_RETURN_IF_ERROR(node->Init(true));
  return node;
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
    // TODO(philkuz) (PL-402) hack to make group all work.
    case AstType::Name: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessNameData(PYPA_PTR_CAST(Name, ast)));
      break;
    }
    default: {
      std::string err_msg =
          absl::StrFormat("Couldn't find %s in ProcessData", GetAstTypeName(ast->type));
      return CreateAstError(err_msg, ast);
    }
  }
  ir_node->SetLineCol(ast->line, ast->column);
  return ir_node;
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
