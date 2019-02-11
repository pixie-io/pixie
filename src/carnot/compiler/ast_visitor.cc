#include "src/carnot/compiler/ast_visitor.h"

namespace pl {
namespace carnot {
namespace compiler {
using pypa::AstType;
using pypa::walk_tree;

#define PYPA_PTR_CAST(TYPE, VAL) \
  std::static_pointer_cast<typename pypa::AstTypeByID<AstType::TYPE>::Type>(VAL)

#define PYPA_CAST(TYPE, VAL) static_cast<AstTypeByID<AstType::TYPE>::Type&>(VAL)

/**
 * @brief Temporary error msg for debugging.
 * Requires people to look in ast_type to find the
 * meaning of the macro.
 *
 * TODO(philkuz) (PL-335) maybe use the weird macro setup to create a
 * dictionary that would return the ast_type.inl from an enum value.
 *
 * @param ap the ptr to the ast.
 */
std::string astTypeErrorMsg(pypa::AstPtr ap) {
  return "Node defined at line " + std::to_string(static_cast<int>(ap->type)) + std::to_string(14) +
         " in <pypa/ast/ast_type.inl> not" + " used as a function node.";
}
/**
 * @brief Create an error that incorporates line, column of node into the error message.
 *
 * @param err_msg
 * @param ast
 * @return Status
 */
Status CreateAstError(const std::string& err_msg, pypa::AstPtr ast) {
  return error::InvalidArgument("Line $0 Col $1 : $2", ast->line, ast->column, err_msg);
}

ASTWalker::ASTWalker(std::shared_ptr<IR> ir_graph) {
  ir_graph_ = ir_graph;
  var_table_ = VarTable();
}

Status ASTWalker::ProcessExprStmtNode(pypa::AstExpressionStatementPtr e) {
  switch (e->expr->type) {
    case AstType::Call:
      return ProcessCallNode(PYPA_PTR_CAST(Call, e->expr)).status();
    default:
      return CreateAstError("Expression node not defined", e);
  }
}

Status ASTWalker::ProcessModuleNode(pypa::AstModulePtr m) {
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
        std::string err_msg = absl::StrFormat("Can't use the %s", astTypeErrorMsg(stmt));
        return CreateAstError(err_msg, m);
    }
  }
  return Status::OK();
}
Status ASTWalker::ProcessAssignNode(pypa::AstAssignPtr node) {
  // Check # nodes to assign.
  if (node->targets.size() != 1) {
    return CreateAstError("AssignNodes are only supported with one target.", node);
  }
  // Get the name that we are targeting.
  auto expr_node = node->targets[0];
  if (expr_node->type != AstType::Name) {
    return CreateAstError("Assign target must be a Name node.", expr_node);
  }
  std::string assign_name = PYPA_PTR_CAST(Name, expr_node)->id;

  // Get the object that we want to assign.
  if (node->value->type != AstType::Call) {
    return CreateAstError("Assign value must be a function call.", node->value);
  }
  StatusOr<IRNode*> value = ProcessCallNode(PYPA_PTR_CAST(Call, node->value));
  PL_RETURN_IF_ERROR(value);

  var_table_[assign_name] = value.ValueOrDie();
  return Status::OK();
}

StatusOr<IRNode*> ASTWalker::ProcessCallNode(pypa::AstCallPtr node) {
  std::string func_name;
  switch (node->function->type) {
    case AstType::Name:
      func_name = PYPA_PTR_CAST(Name, node->function)->id;
      return ProcessFunc(func_name, node);
    case AstType::Attribute:
      func_name = PYPA_PTR_CAST(Name, PYPA_PTR_CAST(Attribute, node->function)->attribute)->id;
      return ProcessFunc(func_name, node);
    default:
      return error::InvalidArgument(astTypeErrorMsg(node->function));
  }
}

StatusOr<ArgMap> ASTWalker::GetArgs(pypa::AstCallPtr call_ast,
                                    std::vector<std::string> expected_args, bool kwargs_only) {
  auto arg_ast = call_ast->arglist;
  if (!kwargs_only) {
    // TODO(philkuz) implement non-kwargs setup. (MS3).
    return error::Unimplemented("Only supporting kwargs for now.");
  }
  // TODO(philkuz) check that non-kw args match the number of expected args. (MS3).
  // TODO(philkuz) check that args aren't defined twice in the unnamed args and the kwargs. (MS3).
  ArgMap arg_map;
  // Set to keep track of args that are not yet found.
  std::unordered_set<std::string> missing_args;
  missing_args.insert(expected_args.begin(), expected_args.end());

  // Iterate through the keywords
  for (auto& k : arg_ast.keywords) {
    pypa::AstKeywordPtr kw_ptr = PYPA_PTR_CAST(Keyword, k);
    std::string key = PYPA_PTR_CAST(Name, kw_ptr->name)->id;
    if (missing_args.find(key) == missing_args.end()) {
      return CreateAstError(absl::Substitute("Keyword '$0' not expected in function.", key),
                            call_ast);
    }
    missing_args.erase(missing_args.find(key));
    IRNode* value = ProcessDataNode(kw_ptr->value).ValueOrDie();
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
StatusOr<IRNode*> ASTWalker::LookupName(pypa::AstNamePtr name_node) {
  // if doesn't exist, then
  auto find_name = var_table_.find(name_node->id);
  if (find_name == var_table_.end()) {
    std::string err_msg = absl::StrFormat("Can't find variable \"%s\".", name_node->id);
    return CreateAstError(err_msg, name_node);
  }
  IRNode* node = find_name->second;

  return node;
}

StatusOr<IRNode*> ASTWalker::ProcessFromOp(pypa::AstCallPtr node) {
  PL_ASSIGN_OR_RETURN(MemorySourceIR * ir_node, ir_graph_->MakeNode<MemorySourceIR>());
  // Get the arguments in the node.
  PL_ASSIGN_OR_RETURN(ArgMap args, GetArgs(node, {"table", "select"}, true));
  PL_RETURN_IF_ERROR(ir_node->Init(args["table"], args["select"]));
  return ir_node;
}
StatusOr<IRNode*> ASTWalker::ProcessRangeOp(pypa::AstCallPtr node) {
  PL_ASSIGN_OR_RETURN(RangeIR * ir_node, ir_graph_->MakeNode<RangeIR>());
  PL_ASSIGN_OR_RETURN(ArgMap args, GetArgs(node, {"time"}, true));
  // TODO(philkuz) this is under the assumption that the Range is always called as an attribute.
  // (MS3) fix.
  // Will have to change after Milestone 2.
  pypa::AstAttributePtr attr = PYPA_PTR_CAST(Attribute, node->function);
  StatusOr<IRNode*> call_result;
  if (attr->value->type == AstType::Call) {
    call_result = ProcessCallNode(PYPA_PTR_CAST(Call, attr->value));
  } else if (attr->value->type == AstType::Name) {
    call_result = LookupName(PYPA_PTR_CAST(Name, attr->value));
  } else {
    return CreateAstError("Can't handle the attribute of this type", attr->value);
  }

  PL_RETURN_IF_ERROR(call_result);
  PL_RETURN_IF_ERROR(ir_node->Init(call_result.ValueOrDie(), args["time"]));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessFunc(const std::string func_name, pypa::AstCallPtr node) {
  IRNode* ir_node;
  if (func_name == kFromOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessFromOp(node));
  } else if (func_name == kRangeOpId) {
    PL_ASSIGN_OR_RETURN(ir_node, ProcessRangeOp(node));
  } else {
    std::string err_msg = absl::Substitute("No function named '$0'", func_name);
    return CreateAstError(err_msg, node);
  }
  ir_node->SetLineCol(node->line, node->column);
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessStrDataNode(pypa::AstStrPtr ast) {
  StringIR* ir_node = ir_graph_->MakeNode<StringIR>().ValueOrDie();
  PL_RETURN_IF_ERROR(ir_node->Init(ast->value));
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessListDataNode(pypa::AstListPtr ast) {
  ListIR* ir_node = ir_graph_->MakeNode<ListIR>().ValueOrDie();
  for (auto& child : ast->elements) {
    PL_ASSIGN_OR_RETURN(IRNode * child_node, ProcessDataNode(child));
    PL_RETURN_IF_ERROR(ir_node->AddListItem(child_node));
  }
  return ir_node;
}

StatusOr<IRNode*> ASTWalker::ProcessDataNode(pypa::AstPtr ast) {
  IRNode* ir_node;
  switch (ast->type) {
    case AstType::Str: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessStrDataNode(PYPA_PTR_CAST(Str, ast)));
      break;
    }
    case AstType::List: {
      PL_ASSIGN_OR_RETURN(ir_node, ProcessListDataNode(PYPA_PTR_CAST(List, ast)));
      break;
    }
    default: {
      std::string err_msg =
          absl::Substitute("Coudln't find $0 in ProcessDataNode", astTypeErrorMsg(ast));
      return CreateAstError(err_msg, ast);
    }
  }
  ir_node->SetLineCol(ast->line, ast->column);
  return ir_node;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
