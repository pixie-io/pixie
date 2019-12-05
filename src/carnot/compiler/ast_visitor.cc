#include "src/carnot/compiler/ast_visitor.h"

#include "src/carnot/compiler/compiler_error_context/compiler_error_context.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/pl_module.h"
#include "src/carnot/compiler/parser/parser.h"

namespace pl {
namespace carnot {
namespace compiler {
using pypa::AstType;

StatusOr<FuncIR::Op> ASTVisitorImpl::GetOp(const std::string& python_op, const pypa::AstPtr node) {
  auto op_find = FuncIR::op_map.find(python_op);
  if (op_find == FuncIR::op_map.end()) {
    return CreateAstError(node, "Operator '$0' not handled", python_op);
  }
  return op_find->second;
}

StatusOr<std::shared_ptr<ASTVisitorImpl>> ASTVisitorImpl::Create(IR* ir_graph,
                                                                 CompilerState* compiler_state) {
  std::shared_ptr<ASTVisitorImpl> ast_visitor =
      std::shared_ptr<ASTVisitorImpl>(new ASTVisitorImpl(ir_graph, compiler_state));
  PL_RETURN_IF_ERROR(ast_visitor->Init());
  return ast_visitor;
}

Status ASTVisitorImpl::Init() {
  var_table_ = VarTable();
  PL_ASSIGN_OR_RETURN(var_table_[kPLModuleObjName], PLModule::Create(ir_graph_, compiler_state_));
  return Status::OK();
}

Status ASTVisitorImpl::ProcessExprStmtNode(const pypa::AstExpressionStatementPtr& e) {
  OperatorContext op_context({}, "", {});
  return Process(e->expr, op_context).status();
}

StatusOr<IRNode*> ASTVisitorImpl::ProcessSingleExpressionModule(const pypa::AstModulePtr& module) {
  OperatorContext op_context({}, "");
  const std::vector<pypa::AstStmt>& items_list = module->body->items;
  if (items_list.size() != 1) {
    return CreateAstError(module,
                          "ProcessModuleExpression only works for single lined statements.");
  }
  const pypa::AstStmt& stmt = items_list[0];
  switch (stmt->type) {
    case pypa::AstType::ExpressionStatement: {
      return ProcessData(PYPA_PTR_CAST(ExpressionStatement, stmt)->expr, op_context);
      break;
    }
    default: {
      return CreateAstError(module, "Want expression, got $0", GetAstTypeName(stmt->type));
    }
  }
}

StatusOr<IRNode*> ASTVisitorImpl::ParseAndProcessSingleExpression(
    std::string_view single_expr_str) {
  Parser parser;
  // TODO(philkuz) switch over parser to std::string_view.
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(single_expr_str.data()));
  return ProcessSingleExpressionModule(ast);
}

Status ASTVisitorImpl::ProcessModuleNode(const pypa::AstModulePtr& m) {
  pypa::AstStmtList items_list = m->body->items;
  if (items_list.size() == 0) {
    return CreateAstError(m, "No runnable code found");
  }
  // iterate through all the items on this list.
  for (pypa::AstStmt stmt : items_list) {
    switch (stmt->type) {
      case pypa::AstType::ExpressionStatement: {
        PL_RETURN_IF_ERROR(ProcessExprStmtNode(PYPA_PTR_CAST(ExpressionStatement, stmt)));
        break;
      }
      case pypa::AstType::Assign: {
        PL_RETURN_IF_ERROR(ProcessAssignNode(PYPA_PTR_CAST(Assign, stmt)));
        break;
      }
      default: {
        return CreateAstError(m, "Can't parse expression of type $0", GetAstTypeName(stmt->type));
      }
    }
  }
  return Status::OK();
}

Status ASTVisitorImpl::ProcessSubscriptMapAssignment(const pypa::AstSubscriptPtr& subscript,
                                                     const pypa::AstPtr& expr_node) {
  if (subscript->value->type != AstType::Name) {
    return CreateAstError(expr_node, "Subscript must be on a Name node, received: $0",
                          GetAstTypeName(subscript->value->type));
  }
  auto assign_name = PYPA_PTR_CAST(Name, subscript->value);
  auto assign_name_string = GetNameAsString(subscript->value);

  // Check to make sure this dataframe exists
  PL_ASSIGN_OR_RETURN(auto parent_op, LookupName(assign_name));

  if (subscript->slice->type != AstType::Index) {
    return CreateAstError(subscript->slice,
                          "Expected to receive index as subscript slice, received $0.",
                          GetAstTypeName(subscript->slice->type));
  }
  auto index = PYPA_PTR_CAST(Index, subscript->slice);
  if (index->value->type != AstType::Str) {
    return CreateAstError(index->value,
                          "Expected to receive string as subscript index value, received $0.",
                          GetAstTypeName(index->value->type));
  }
  auto col_name = PYPA_PTR_CAST(Str, index->value)->value;

  // Maps can only assign to the same table as the input table when of the form:
  // df['foo'] = df['bar'] + 2
  OperatorContext op_context{{parent_op}, Dataframe::kMapOpId, {assign_name_string}};
  PL_ASSIGN_OR_RETURN(auto result, ProcessData(expr_node, op_context));
  if (!result->IsExpression()) {
    return CreateAstError(
        expr_node, "Expected to receive expression as map subscript assignment value, received $0.",
        GetAstTypeName(index->value->type));
  }
  auto expr = static_cast<ExpressionIR*>(result);

  // Pull in all columns needed in fn.
  ColExpressionVector map_exprs{{col_name, expr}};
  PL_ASSIGN_OR_RETURN(MapIR * ir_node, ir_graph_->CreateNode<MapIR>(expr_node, parent_op, map_exprs,
                                                                    /*keep_input_cols*/ true));
  PL_ASSIGN_OR_RETURN(var_table_[assign_name_string], Dataframe::Create(ir_node));

  return Status::OK();
}

StatusOr<QLObjectPtr> ASTVisitorImpl::Process(const pypa::AstExpr& node,
                                              const OperatorContext& op_context) {
  switch (node->type) {
    case AstType::Call:
      return ProcessCallNode(PYPA_PTR_CAST(Call, node), op_context);
    case AstType::Subscript:
      return ProcessSubscriptCall(PYPA_PTR_CAST(Subscript, node), op_context);
    case AstType::Name:
      return LookupVariable(PYPA_PTR_CAST(Name, node));
    case AstType::Attribute:
      return ProcessAttribute(PYPA_PTR_CAST(Attribute, node), op_context);
    default:
      return CreateAstError(node, "Expression node '$0' not defined", GetAstTypeName(node->type));
  }
}

Status ASTVisitorImpl::ProcessAssignNode(const pypa::AstAssignPtr& node) {
  // Check # nodes to assign.
  if (node->targets.size() != 1) {
    return CreateAstError(node, "We only support single target assignment.");
  }
  // Get the name that we are targeting.
  auto target_node = node->targets[0];

  // Special handler for this type of map statement: df['foo'] = df['bar']
  if (target_node->type == AstType::Subscript) {
    return ProcessSubscriptMapAssignment(PYPA_PTR_CAST(Subscript, node->targets[0]), node->value);
  }

  if (target_node->type != AstType::Name) {
    return CreateAstError(target_node, "Assignment target must be a Name or Subscript");
  }

  std::string assign_name = GetNameAsString(target_node);
  OperatorContext op_context({}, "", {});
  PL_ASSIGN_OR_RETURN(var_table_[assign_name], Process(node->value, op_context));
  return Status::OK();
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessSubscriptCall(const pypa::AstSubscriptPtr& node,
                                                           const OperatorContext& op_context) {
  PL_ASSIGN_OR_RETURN(QLObjectPtr pyobject, Process(node->value, op_context));
  if (!pyobject->HasSubscriptMethod()) {
    return pyobject->CreateError("$0 object is not subscriptable");
  }
  PL_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> func_object, pyobject->GetSubscriptMethod());

  auto slice = node->slice;
  if (slice->type != AstType::Index) {
    return CreateAstError(slice, "'$0' object cannot be an index", GetAstTypeName(slice->type));
  }

  std::vector<std::string> dfs = op_context.referenceable_dataframes;
  if (node->value->type == AstType::Name) {
    dfs.push_back(GetNameAsString(node->value));
  }

  OperatorContext new_op_context(op_context.parent_ops, op_context.operator_name, dfs);
  PL_ASSIGN_OR_RETURN(IRNode * ir_node,
                      ProcessData(PYPA_PTR_CAST(Index, slice)->value, new_op_context));
  ArgMap args;
  args.args.push_back(ir_node);
  return func_object->Call(args, node, this);
}

StatusOr<std::string> ASTVisitorImpl::GetFuncName(const pypa::AstCallPtr& node) {
  std::string func_name;
  switch (node->function->type) {
    case AstType::Name: {
      func_name = GetNameAsString(node->function);
      break;
    }
    case AstType::Attribute: {
      auto attr = PYPA_PTR_CAST(Attribute, node->function);
      if (attr->attribute->type != AstType::Name) {
        return CreateAstError(node->function, "Couldn't get string name out of node of type $0.",
                              GetAstTypeName(attr->attribute->type));
      }
      func_name = GetNameAsString(attr->attribute);
      break;
    }
    default: {
      return CreateAstError(node->function, "Couldn't get string name out of node of type $0.",
                            GetAstTypeName(node->function->type));
    }
  }
  return func_name;
}

StatusOr<ArgMap> ASTVisitorImpl::ProcessArgs(const pypa::AstCallPtr& call_ast) {
  OperatorContext op_context({}, "");
  return ProcessArgs(call_ast, op_context);
}

StatusOr<ArgMap> ASTVisitorImpl::ProcessArgs(const pypa::AstCallPtr& call_ast,
                                             const OperatorContext& op_context) {
  auto arg_ast = call_ast->arglist;
  ArgMap arg_map;

  for (const auto arg : arg_ast.arguments) {
    PL_ASSIGN_OR_RETURN(IRNode * value, ProcessData(arg, op_context));
    arg_map.args.push_back(value);
  }

  // Iterate through the keywords
  for (auto& k : arg_ast.keywords) {
    pypa::AstKeywordPtr kw_ptr = PYPA_PTR_CAST(Keyword, k);
    std::string key = GetNameAsString(kw_ptr->name);
    PL_ASSIGN_OR_RETURN(IRNode * value, ProcessData(kw_ptr->value, op_context));
    arg_map.kwargs.emplace_back(key, value);
  }

  return arg_map;
}

StatusOr<QLObjectPtr> ASTVisitorImpl::LookupVariable(const pypa::AstPtr& ast,
                                                     const std::string& name) {
  auto find_name = var_table_.find(name);
  if (find_name == var_table_.end()) {
    return CreateAstError(ast, "name '$0' is not defined", name);
  }
  return find_name->second;
}

StatusOr<OperatorIR*> ASTVisitorImpl::LookupName(const pypa::AstNamePtr& name_node) {
  PL_ASSIGN_OR_RETURN(QLObjectPtr pyobject, LookupVariable(name_node));
  if (!pyobject->HasNode()) {
    return CreateAstError(name_node, "'$0' not accessible", name_node->id);
  }
  IRNode* node = pyobject->node();
  if (!node->IsOperator()) {
    return node->CreateIRNodeError("Only dataframes may be assigned variables, $0 not allowed",
                                   node->type_string());
  }
  return static_cast<OperatorIR*>(node);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessAttribute(const pypa::AstAttributePtr& node,
                                                       const OperatorContext& op_context) {
  PL_ASSIGN_OR_RETURN(std::string attr_name, GetAttributeStr(node));
  PL_ASSIGN_OR_RETURN(QLObjectPtr value_obj, Process(node->value, op_context));
  return value_obj->GetAttribute(node, attr_name);
}

StatusOr<std::string> ASTVisitorImpl::GetAttributeStr(const pypa::AstAttributePtr& attr) {
  if (attr->attribute->type != AstType::Name) {
    return CreateAstError(attr, "$0 not a valid attribute", GetAstTypeName(attr->attribute->type));
  }
  return GetNameAsString(attr->attribute);
}

StatusOr<QLObjectPtr> ASTVisitorImpl::ProcessCallNode(const pypa::AstCallPtr& node,
                                                      const OperatorContext& op_context) {
  std::shared_ptr<FuncObject> func_object;
  // pyobject declared up here because we need this object to be allocated when
  // func_object->Call() is made.
  PL_ASSIGN_OR_RETURN(QLObjectPtr pyobject, Process(node->function, op_context));
  if (pyobject->type_descriptor().type() != QLObjectType::kFunction) {
    PL_ASSIGN_OR_RETURN(func_object, pyobject->GetCallMethod());
  } else {
    func_object = std::static_pointer_cast<FuncObject>(pyobject);
  }
  PL_ASSIGN_OR_RETURN(ArgMap args, ProcessArgs(node, op_context));
  return func_object->Call(args, node, this);
}

StatusOr<ExpressionIR*> ASTVisitorImpl::ProcessStr(const pypa::AstStrPtr& ast) {
  PL_ASSIGN_OR_RETURN(auto str_value, GetStrAstValue(ast));
  return ir_graph_->CreateNode<StringIR>(ast, str_value);
}

StatusOr<std::vector<ExpressionIR*>> ASTVisitorImpl::ProcessCollectionChildren(
    const pypa::AstExprList& elements, const OperatorContext& op_context) {
  std::vector<ExpressionIR*> children;
  for (auto& child : elements) {
    PL_ASSIGN_OR_RETURN(IRNode * child_node, ProcessData(child, op_context));
    if (!child_node->IsExpression()) {
      return CreateAstError(child, "Can't support '$0' as a Collection member.",
                            child_node->type_string());
    }
    children.push_back(static_cast<ExpressionIR*>(child_node));
  }
  return children;
}

StatusOr<ListIR*> ASTVisitorImpl::ProcessList(const pypa::AstListPtr& ast,
                                              const OperatorContext& op_context) {
  PL_ASSIGN_OR_RETURN(std::vector<ExpressionIR*> expr_vec,
                      ProcessCollectionChildren(ast->elements, op_context));
  return ir_graph_->CreateNode<ListIR>(ast, expr_vec);
}

StatusOr<TupleIR*> ASTVisitorImpl::ProcessTuple(const pypa::AstTuplePtr& ast,
                                                const OperatorContext& op_context) {
  PL_ASSIGN_OR_RETURN(std::vector<ExpressionIR*> expr_vec,
                      ProcessCollectionChildren(ast->elements, op_context));
  return ir_graph_->CreateNode<TupleIR>(ast, expr_vec);
}

StatusOr<ExpressionIR*> ASTVisitorImpl::ProcessNumber(const pypa::AstNumberPtr& node) {
  switch (node->num_type) {
    case pypa::AstNumber::Type::Float: {
      return ir_graph_->CreateNode<FloatIR>(node, node->floating);
    }
    case pypa::AstNumber::Type::Integer:
    case pypa::AstNumber::Type::Long: {
      return ir_graph_->CreateNode<IntIR>(node, node->integer);
    }
    default:
      return CreateAstError(node, "Couldn't find number type $0", node->num_type);
  }
}

StatusOr<ExpressionIR*> ASTVisitorImpl::ProcessDataBinOp(const pypa::AstBinOpPtr& node,
                                                         const OperatorContext& op_context) {
  std::string op_str = pypa::to_string(node->op);

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

  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<ExpressionIR*> expressions = {static_cast<ExpressionIR*>(left),
                                            static_cast<ExpressionIR*>(right)};
  return ir_graph_->CreateNode<FuncIR>(node, op, expressions);
}

StatusOr<ExpressionIR*> ASTVisitorImpl::ProcessDataBoolOp(const pypa::AstBoolOpPtr& node,
                                                          const OperatorContext& op_context) {
  std::string op_str = pypa::to_string(node->op);
  if (node->values.size() != 2) {
    return CreateAstError(node, "Expected two arguments to '$0'.", op_str);
  }

  PL_ASSIGN_OR_RETURN(IRNode * left, ProcessData(node->values[0], op_context));
  PL_ASSIGN_OR_RETURN(IRNode * right, ProcessData(node->values[1], op_context));
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

  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  std::vector<ExpressionIR*> expressions = {static_cast<ExpressionIR*>(left),
                                            static_cast<ExpressionIR*>(right)};
  return ir_graph_->CreateNode<FuncIR>(node, op, expressions);
}

StatusOr<ExpressionIR*> ASTVisitorImpl::ProcessDataCompare(const pypa::AstComparePtr& node,
                                                           const OperatorContext& op_context) {
  DCHECK_EQ(node->operators.size(), 1ULL);
  std::string op_str = pypa::to_string(node->operators[0]);
  if (node->comparators.size() != 1) {
    return CreateAstError(node, "Only expected one argument to the right of '$0'.", op_str);
  }
  PL_ASSIGN_OR_RETURN(IRNode * left, ProcessData(node->left, op_context));
  if (!left->IsExpression()) {
    return CreateAstError(
        node,
        "Expected left side of operation to be an expression, but got $0, which is not an "
        "expression..",
        left->type_string());
  }
  std::vector<ExpressionIR*> expressions = {static_cast<ExpressionIR*>(left)};

  for (const auto& comp : node->comparators) {
    PL_ASSIGN_OR_RETURN(IRNode * expr, ProcessData(comp, op_context));
    if (!expr->IsExpression()) {
      return CreateAstError(comp, "Expected expression, but got $0, which is not an expression.",
                            expr->type_string());
    }
    expressions.push_back(static_cast<ExpressionIR*>(expr));
  }

  PL_ASSIGN_OR_RETURN(FuncIR::Op op, GetOp(op_str, node));
  return ir_graph_->CreateNode<FuncIR>(node, op, expressions);
}

StatusOr<ColumnIR*> ASTVisitorImpl::ProcessSubscriptColumnWithAttribute(
    const pypa::AstSubscriptPtr& subscript, const OperatorContext& op_context) {
  if (subscript->value->type != AstType::Attribute) {
    return CreateAstError(subscript, "expected attribute, got $0",
                          GetAstTypeName(subscript->value->type));
  }

  PL_ASSIGN_OR_RETURN(QLObjectPtr ptr, Process(subscript->value, op_context));

  // TODO(philkuz) check the line, column of this error to make sure it's here, otherwise call the
  // error here.
  PL_ASSIGN_OR_RETURN(std::shared_ptr<FuncObject> func, ptr->GetSubscriptMethod());

  if (subscript->slice->type != AstType::Index) {
    return CreateAstError(subscript->slice,
                          "Expected to receive index as subscript slice, received $0.",
                          GetAstTypeName(subscript->slice->type));
  }
  auto index = PYPA_PTR_CAST(Index, subscript->slice);
  PL_ASSIGN_OR_RETURN(IRNode * data, ProcessData(index->value, op_context));
  PL_ASSIGN_OR_RETURN(QLObjectPtr func_result, func->Call(ArgMap{{}, {data}}, subscript, this));
  if (!func_result->HasNode()) {
    return CreateAstError(subscript, "subscript operator does not return any value");
  }

  IRNode* out_node = func_result->node();
  if (!Match(out_node, ColumnNode())) {
    return CreateAstError(subscript, "subscript result isn't a column, received a $0",
                          out_node->type_string());
  }

  return static_cast<ColumnIR*>(out_node);
}

StatusOr<ColumnIR*> ASTVisitorImpl::ProcessSubscriptColumn(const pypa::AstSubscriptPtr& subscript,
                                                           const OperatorContext& op_context) {
  // TODO(philkuz) generalize this approach by support "load" subscripts for Dataframes.
  if (subscript->value->type == AstType::Attribute) {
    return ProcessSubscriptColumnWithAttribute(subscript, op_context);
  }

  // TODO(nserrino) support indexing into lists and other things like that.
  if (subscript->value->type != AstType::Name) {
    return CreateAstError(subscript->value,
                          "Subscript is only currently supported on dataframes, received $0.",
                          GetAstTypeName(subscript->value->type));
  }

  auto name = PYPA_PTR_CAST(Name, subscript->value);
  if (std::find(op_context.referenceable_dataframes.begin(),
                op_context.referenceable_dataframes.end(),
                name->id) == op_context.referenceable_dataframes.end()) {
    return CreateAstError(name, "name '$0' is not available in this context", name->id);
  }

  if (subscript->slice->type != AstType::Index) {
    return CreateAstError(subscript->slice,
                          "Expected to receive index as subscript slice, received $0.",
                          GetAstTypeName(subscript->slice->type));
  }
  auto index = PYPA_PTR_CAST(Index, subscript->slice);
  if (index->value->type != AstType::Str) {
    return CreateAstError(index->value,
                          "Expected to receive string as subscript index value, received $0.",
                          GetAstTypeName(index->value->type));
  }
  auto col_name = PYPA_PTR_CAST(Str, index->value)->value;

  return ir_graph_->CreateNode<ColumnIR>(subscript, col_name, /* parent_op_idx */ 0);
}

StatusOr<IRNode*> ASTVisitorImpl::ProcessDataForAttribute(const pypa::AstAttributePtr& attr) {
  if (attr->value->type != AstType::Name) {
    // TODO(philkuz) support more complex cases here when they come
    return CreateAstError(attr, "Attributes can only be one layer deep for now");
  }

  PL_ASSIGN_OR_RETURN(QLObjectPtr object, LookupVariable(PYPA_PTR_CAST(Name, attr->value)));
  std::string attr_str = GetNameAsString(attr->attribute);
  PL_ASSIGN_OR_RETURN(QLObjectPtr attr_object, object->GetAttribute(attr->attribute, attr_str));
  if (!attr_object->HasNode()) {
    // TODO(philkuz) refactor ArgMap/ParsedArgs to push the function calls to the handler instead of
    // this hack that only works for pl modules.
    QLObjectType ql_object_type = object->type_descriptor().type();
    QLObjectType attr_object_type = attr_object->type_descriptor().type();
    if (ql_object_type != QLObjectType::kPLModule && attr_object_type != QLObjectType::kFunction) {
      return CreateAstError(attr->attribute, "does not return a usable value");
    }

    PL_ASSIGN_OR_RETURN(
        attr_object,
        std::static_pointer_cast<FuncObject>(attr_object)->Call({}, attr->attribute, this));
    if (!attr_object->HasNode()) {
      return CreateAstError(attr->attribute, "does not return a usable value");
    }
  }
  return attr_object->node();
}

StatusOr<IRNode*> ASTVisitorImpl::ProcessData(const pypa::AstPtr& ast,
                                              const OperatorContext& op_context) {
  switch (ast->type) {
    case AstType::Str: {
      return ProcessStr(PYPA_PTR_CAST(Str, ast));
    }
    case AstType::Number: {
      return ProcessNumber(PYPA_PTR_CAST(Number, ast));
    }
    case AstType::List: {
      return ProcessList(PYPA_PTR_CAST(List, ast), op_context);
    }
    case AstType::Tuple: {
      return ProcessTuple(PYPA_PTR_CAST(Tuple, ast), op_context);
    }
    case AstType::Call: {
      PL_ASSIGN_OR_RETURN(QLObjectPtr call_result,
                          ProcessCallNode(PYPA_PTR_CAST(Call, ast), op_context));
      DCHECK(call_result->HasNode());
      return call_result->node();
    }
    case AstType::BinOp: {
      return ProcessDataBinOp(PYPA_PTR_CAST(BinOp, ast), op_context);
    }
    case AstType::BoolOp: {
      return ProcessDataBoolOp(PYPA_PTR_CAST(BoolOp, ast), op_context);
    }
    case AstType::Compare: {
      return ProcessDataCompare(PYPA_PTR_CAST(Compare, ast), op_context);
    }
    case AstType::Name: {
      return LookupName(PYPA_PTR_CAST(Name, ast));
    }
    case AstType::Subscript: {
      return ProcessSubscriptColumn(PYPA_PTR_CAST(Subscript, ast), op_context);
    }
    case AstType::Attribute: {
      return ProcessDataForAttribute(PYPA_PTR_CAST(Attribute, ast));
    }
    default: {
      return CreateAstError(ast, "Couldn't find $0 in ProcessData", GetAstTypeName(ast->type));
    }
  }
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
