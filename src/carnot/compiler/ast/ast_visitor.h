#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/plannerpb/query_flags.pb.h"

namespace pl {
namespace carnot {
namespace compiler {

class ASTVisitor {
 public:
  // Interface boilerplate.
  virtual ~ASTVisitor() = default;

  /**
   * @brief The entry point into traversal as the root AST is a module.
   *
   * @param m the ptr to the ast node
   * @return Status
   */
  virtual Status ProcessModuleNode(const pypa::AstModulePtr& m) = 0;

  /**
   * @brief Processes a single expression into an IRNode. If the parameter has more than one
   * line or contains a module we can't process, then it will error out.
   *
   * @param m the ptr to the ast node
   * @param op_context the operator context to operate with.
   * @return StatusOr<IRNode*> the graph representation of the expression, or an error if the
   * operator fails.
   *
   * TODO(nserrino, philkuz): Have this return a QLObjectPtr instead of an IRNode so that non
   * IRNode objects (like int or None) can be processed here.
   */
  virtual StatusOr<IRNode*> ProcessSingleExpressionModule(const pypa::AstModulePtr& m) = 0;

  /**
   * @brief Parses and processes out a single expression in the form of an IRNode.
   *
   * @param str the input string
   * @return StatusOr<IRNode*> the IR of the expression or an error if something fails during
   * processing.
   *
   * TODO(nserrino, philkuz): Same as above, have this return a QLObjectPtr instead of an IRNode
   * so that non IRNode objects (like int or None) can be processed here.
   */
  virtual StatusOr<IRNode*> ParseAndProcessSingleExpression(std::string_view str) = 0;

  /**
   * @brief Parses the AST for the available flags (default, description, etc).
   *
   * @param m the ptr to the ast node
   * @return StatusOr<plannerpb::QueryFlagsSpec> the available flags
   *
   */
  virtual StatusOr<plannerpb::QueryFlagsSpec> GetAvailableFlags(const pypa::AstModulePtr& m) = 0;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
