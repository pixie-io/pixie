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

#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/plannerpb/service.pb.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

// Forward declare;
class QLObject;
using QLObjectPtr = std::shared_ptr<QLObject>;
class VarTable;

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
   * @return StatusOr<QLObjectPtr> the QL object representation of the expression, or an error if
   * the operator fails.
   */
  virtual StatusOr<QLObjectPtr> ProcessSingleExpressionModule(const pypa::AstModulePtr& m) = 0;

  /**
   * @brief Parses and processes out a single expression in the form of an IRNode.
   *
   * @param str the input string
   * @param import_px whether or not to import the pixie module first.
   * @return StatusOr<QLObjectPtr> the QL object representation of the expression or an error if
   * something fails during processing.
   */
  virtual StatusOr<QLObjectPtr> ParseAndProcessSingleExpression(std::string_view str,
                                                                bool import_px) = 0;

  /**
   * @brief Process the functions to execute.
   *
   * @return Status
   */
  virtual Status ProcessExecFuncs(const std::vector<plannerpb::FuncToExecute>& exec_funcs) = 0;

  /**
   * @brief Creates a child of this visitor specifically for use to parse modules.
   * Any internal state will be stored in var_table.
   *
   * @param var_table the var table to use for internal parsing.
   * @return std::shared_ptr<ASTVisitor> The Child Module Visitor.
   */
  virtual std::shared_ptr<ASTVisitor> CreateModuleVisitor(std::shared_ptr<VarTable> var_table) = 0;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
