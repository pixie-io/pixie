#pragma once

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

constexpr const char* kUDFPrefix = "pl";

constexpr const char* kFromOpId = "From";
constexpr const char* kRangeOpId = "Range";
constexpr const char* kMapOpId = "Map";

using VarTable = std::unordered_map<std::string, IRNode*>;
using ArgMap = std::unordered_map<std::string, IRNode*>;
/**
 * @brief Struct that packages the column names and the expr within the function.
 *
 */
struct LambdaExprReturn {
  LambdaExprReturn() {}
  explicit LambdaExprReturn(const std::string& str) : str_(str) {}
  explicit LambdaExprReturn(IRNode* expr) : expr_(expr) {}
  LambdaExprReturn(IRNode* expr, std::unordered_set<std::string> column_names)
      : input_relation_columns_(column_names), expr_(expr) {}
  LambdaExprReturn(IRNode* expr, const LambdaExprReturn& left_expr_ret,
                   const LambdaExprReturn& right_expr_ret)
      : expr_(expr) {
    auto left_set = left_expr_ret.input_relation_columns_;
    auto right_set = right_expr_ret.input_relation_columns_;
    std::set_union(left_set.begin(), left_set.end(), right_set.begin(), right_set.end(),
                   std::inserter(input_relation_columns_, input_relation_columns_.end()));
  }

  /**
   * @brief Returns a merged unordered_set of the columns with this and `ret`s columns.
   *
   * Does manipulate this unordered_set, but assuming that we don't need LambdaExprReturn to stay
   * constant after it's returned
   *
   * @param ret
   * @return std::unordered_unordered_set<std::string>
   */
  const std::unordered_set<std::string>& MergeColumns(const LambdaExprReturn& ret) {
    input_relation_columns_.insert(ret.input_relation_columns_.begin(),
                                   ret.input_relation_columns_.end());
    return input_relation_columns_;
  }

  // The columns we expect to find in the lambda function.
  std::unordered_set<std::string> input_relation_columns_;
  IRNode* expr_ = nullptr;
  std::string str_;
  bool StringOnly() const { return expr_ == nullptr && !str_.empty(); }
};

struct LambdaBodyReturn {
  Status AddExpr(const std::string& name, IRNode* expr) {
    col_expr_map_[name] = expr;
    return Status::OK();
  }
  Status AddColumns(const std::unordered_set<std::string>& new_columns_) {
    input_relation_columns_.insert(new_columns_.begin(), new_columns_.end());
    return Status::OK();
  }
  Status AddExprResult(const std::string& name, const LambdaExprReturn& expr_result) {
    PL_RETURN_IF_ERROR(AddExpr(name, expr_result.expr_));
    PL_RETURN_IF_ERROR(AddColumns(expr_result.input_relation_columns_));
    return Status::OK();
  }
  std::unordered_set<std::string> input_relation_columns_;
  ColExprMap col_expr_map_;
};

class ASTWalker {
 public:
  /**
   * @brief Construct a new ASTWalker object.
   * This constructor will be used at the top level.
   *
   * @param ir_graph
   */
  explicit ASTWalker(std::shared_ptr<IR> ir_graph);

  std::shared_ptr<IR> ir_graph() const { return ir_graph_; }

  /**
   * @brief The entry point into traversal as the root AST is a module.
   *
   * @param node: the ptr to the ast node.
   * @return Status
   */
  Status ProcessModuleNode(const pypa::AstModulePtr& node);

 private:
  /**
   * @brief GetArgs traverses an arg_ast tree, confirms that the expected_args are found in that
   * tree, and then returns a map of those expected args to the nodes they point to.
   *
   * @param arg_ast The arglist ast
   * @param expected_args The string args are expect. Should be ordered if kwargs_only is false.
   * @param kwargs_only Whether to only allow keyword args.
   * @return StatusOr<ArgMap>
   */
  StatusOr<ArgMap> GetArgs(const pypa::AstCallPtr& arg_ast,
                           const std::vector<std::string>& expected_args, bool kwargs_only);

  /**
   * @brief ProcessExprStmtNode handles full lines that are expression statements.
   * ie in the following lines
   *  1: a =From(...)
   *  2: a.Range(...)
   * Line 1 will be wrapped in an AstAssignNode
   * Line 2 will be wrapped in an AstExpressionStatementNode.
   *
   * The entirety of line 2 is the expression statement and will be handled by this function.
   *
   * @param node
   * @return Status
   */
  Status ProcessExprStmtNode(const pypa::AstExpressionStatementPtr& node);

  /**
   * @brief ProcessAssignNode handles lines where an expression is assigned to a value.
   * ie in the following lines
   *  1: a =From(...)
   *  2: a.Range(...)
   * Line 1 will be wrapped in an AstAssignNode
   * Line 2 will be wrapped in an AstExpressionStatementNode.
   *
   * The entirety of line 1 is the assign statement and will be handled by this function.
   *
   * @param node
   * @return Status
   */
  Status ProcessAssignNode(const pypa::AstAssignPtr& node);

  /**
   * @brief ProcessOpCallNode handles call nodes which are created for any function call
   * ie
   *  Range(...)
   *
   * Meant to handle operators and only extracts the name of the function, then passes to
   * ProcessFunc.
   *
   *
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessOpCallNode(const pypa::AstCallPtr& node);
  /**
   * @brief Processes the From operator.
   *
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessFromOp(const pypa::AstCallPtr& node);

  /**
   * @brief Processes the Range operator.
   *
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessRangeOp(const pypa::AstCallPtr& node);

  /**
   * @brief Processes the Map operator.
   *
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessMapOp(const pypa::AstCallPtr& node);

  /**
   * @brief ProcessFunc handles functions that have already been determined with a name.
   *
   * @param name the name of the function to run.
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessFunc(const std::string& name, const pypa::AstCallPtr& node);

  /**
   * @brief Processes an Attribute ast that is supposed to point to a function.
   *
   * TODO(philkuz) figure out if we can use this in conjunction with the other Attribute parser.
   * @param node attribute node that is known to be a function.
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessAttrFunc(const pypa::AstAttributePtr& node);

  /**
   * @brief Processes a list ptr into an IR node.
   *
   * @param ast
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessListDataNode(const pypa::AstListPtr& ast);

  /**
   * @brief Processes a number into an IR Node.
   *
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<IRNode*> ProcessNumberNode(const pypa::AstNumberPtr& node);

  /**
   * @brief Processes a str ast ptr into an IR node.
   *
   * @param ast
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessStrDataNode(const pypa::AstStrPtr& ast);

  /**
   * @brief ProcessDataNode takes in what are typically function arguments and returns the
   * approriate data representation.
   *
   * Ie it might take in an AST tree that represents a list of strings, and convert that into a
   * ListIR node.
   *
   * TODO(philkuz) maybe a rename is in order?
   *
   * @param ast
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessDataNode(const pypa::AstPtr& ast);

  /**
   * @brief Gets the name string contained within the Name ast node and returns the IRNode
   * referenced by that name, or errors out with an undefined variable.
   *
   * @param name
   * @return StatusOr<IRNode*> - IRNode ptr that was created and handled by the IR
   */
  StatusOr<IRNode*> LookupName(const pypa::AstNamePtr& name);

  /**
   * @brief Processes the Lambda args node and returns the
   * string representation of the argument to be used in processing the lambda body traversal.
   * Makes the assumption that there is only one argument and no funny business with default args.
   *
   * @param node
   * @return StatusOr<std::string>
   */
  StatusOr<std::string> ProcessLambdaArgs(const pypa::AstLambdaPtr& node);

  /**
   * @brief Splits apart the Dictionary contained in the lambda fn,
   * evaluates the values in that dictionary, which should just be expressions,
   * then returns the Expression map and the Body return.
   *
   * @param arg_name : the name of the input argument passed into the lambda.
   * @param node : the node that body points to
   * @return StatusOr<ColExprMap> a map from new column name to expression.
   */
  StatusOr<LambdaBodyReturn> ProcessLambdaDict(const std::string& arg_name,
                                               const pypa::AstDictPtr& node);

  /**
   * @brief Takes a binary operation node and translates it to an IRNode expression.
   *
   * @param arg_name
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaBinOp(const std::string& arg_name,
                                                const pypa::AstBinOpPtr& node);

  /**
   * @brief Takes in an attribute contained within a lambda and maps it to either a column or a
   * function call.
   *
   * @param arg_name
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaAttribute(const std::string& arg_name,
                                                    const pypa::AstAttributePtr& node);

  /**
   * @brief Processes a call node with the lambda context (arg_name) that helps identify and return
   * the column names we want, and notifies us when there is a column name being used
   *
   * @param arg_name the name of the argument of the lambda function which represents a record. Used
   * to identify column names.
   * @param node the node we call.
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaCall(const std::string& arg_name,
                                               const pypa::AstCallPtr& node);

  /**
   * @brief Takes an expression and the lambda arg name, processses the expression into an
   * IRNode, and extracts any expected relation values.
   *
   * @param arg_name
   * @param node
   * @return StatusOr<LambdaExprReturn>
   */
  StatusOr<LambdaExprReturn> ProcessLambdaExpr(const std::string& arg_name,
                                               const pypa::AstPtr& node);

  /**
   * @brief Main entry point for Lambda processing.
   *
   * @param ast
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessLambda(const pypa::AstLambdaPtr& ast);

  std::shared_ptr<IR> ir_graph_;
  VarTable var_table_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
