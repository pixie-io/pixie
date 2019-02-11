#pragma once

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

constexpr const char* kFromOpId = "From";
constexpr const char* kRangeOpId = "Range";
constexpr const char* kMapOpId = "Map";

using VarTable = std::unordered_map<std::string, IRNode*>;
using ArgMap = std::unordered_map<std::string, IRNode*>;
struct IRNodeContainer {
  IRNode* node_;
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
  Status ProcessModuleNode(pypa::AstModulePtr node);

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
  StatusOr<ArgMap> GetArgs(pypa::AstCallPtr arg_ast, std::vector<std::string> expected_args,
                           bool kwargs_only);

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
  Status ProcessExprStmtNode(pypa::AstExpressionStatementPtr node);

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
  Status ProcessAssignNode(pypa::AstAssignPtr node);

  /**
   * @brief ProcessCallNode handles call nodes which are created for any function call
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
  StatusOr<IRNode*> ProcessCallNode(pypa::AstCallPtr node);

  /**
   * @brief Processes the From operator.
   *
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessFromOp(pypa::AstCallPtr node);

  /**
   * @brief Processes the Range operator.
   *
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessRangeOp(pypa::AstCallPtr node);

  /**
   * @brief ProcessFunc handles functions that have already been determined with a name.
   *
   * @param name the name of the function to run.
   * @param node
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessFunc(std::string name, pypa::AstCallPtr node);

  /**
   * @brief Processes a list ptr into an IR node.
   *
   * @param ast
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessListDataNode(pypa::AstListPtr ast);

  /**
   * @brief Processes a str ast ptr into an IR node.
   *
   * @param ast
   * @return StatusOr<IRNode*>
   */
  StatusOr<IRNode*> ProcessStrDataNode(pypa::AstStrPtr ast);

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
  StatusOr<IRNode*> ProcessDataNode(pypa::AstPtr ast);

  /**
   * @brief Gets the name string contained within the Name ast node and returns the IRNode
   * referenced by that name, or errors out with an undefined variable.
   *
   * @param name
   * @return StatusOr<IRNode*> - IRNode ptr that was created and handled by the IR
   */
  StatusOr<IRNode*> LookupName(pypa::AstNamePtr name);

  std::shared_ptr<IR> ir_graph_;
  VarTable var_table_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
