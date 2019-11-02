#include "src/carnot/compiler/objects/dataframe.h"

namespace pl {
namespace carnot {
namespace compiler {
Dataframe::Dataframe(OperatorIR* op) : QLObject(DataframeType, op), op_(op) {
  CHECK(op != nullptr) << "Bad argument in Dataframe constructor.";
  /**
   * # Equivalent to the python method method syntax:
   * def merge(self, right, how, left_on, right_on, suffixes=('_x', '_y')):
   *     ...
   */
  // TODO(philkuz) If there's a chance convert the internals of FuncObject to compile time
  // checking of the default arguments. Everytime we create a Dataframe object you have to make this
  // binding.
  std::shared_ptr<FuncObject> mergefn(new FuncObject(
      kMergeOpId, {"right", "how", "left_on", "right_on", "suffixes"},
      {{"suffixes", "('_x', '_y')"}},
      /* has_kwargs */ false,
      std::bind(&JoinHandler::Eval, this, std::placeholders::_1, std::placeholders::_2)));
  AddMethod(kMergeOpId, mergefn);
}

StatusOr<QLObjectPtr> JoinHandler::Eval(Dataframe* df, const pypa::AstPtr& ast,
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
  if (!Match(suffixes_node, ListWithChildren(String()))) {
    return suffixes_node->CreateIRNodeError(
        "'suffixes' must be a tuple with 2 strings - for the left and right suffixes.");
  }

  PL_ASSIGN_OR_RETURN(std::vector<std::string> suffix_strs,
                      ParseStringListIR(static_cast<ListIR*>(suffixes_node)));
  if (suffix_strs.size() != 2) {
    return suffixes_node->CreateIRNodeError(
        "'suffixes' must be a tuple with 2 elements. Received $0", suffix_strs.size());
  }

  PL_ASSIGN_OR_RETURN(JoinIR * join_op, df->graph()->MakeNode<JoinIR>(ast));
  PL_RETURN_IF_ERROR(
      join_op->Init({df->op(), right}, how_type, left_on_cols, right_on_cols, suffix_strs));
  return StatusOr(std::make_shared<Dataframe>(join_op));
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
      PL_ASSIGN_OR_RETURN(ColumnIR * col, graph->MakeNode<ColumnIR>());
      PL_RETURN_IF_ERROR(col->Init(str->str(), parent_index, str->ast_node()));
      columns[idx] = col;
    }
    return columns;
  } else if (!Match(node, String())) {
    return node->CreateIRNodeError("'$0' must be a label or a list of labels", arg_name);
  }
  StringIR* str = static_cast<StringIR*>(node);
  PL_ASSIGN_OR_RETURN(ColumnIR * col, graph->MakeNode<ColumnIR>());
  PL_RETURN_IF_ERROR(col->Init(str->str(), parent_index, str->ast_node()));
  return std::vector<ColumnIR*>{col};
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
