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

#include <cstdint>
#include <memory>
#include <string>

#include <pypa/ast/ast.hh>

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/types/types.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class IR;
class IRNode;
using IRNodePtr = std::unique_ptr<IRNode>;

enum class IRNodeType {
  kAny = -1,
#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME) k##NAME,
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PX_CARNOT_IR_NODE
  number_of_types  // This is not a real type, but is used to verify strings are inline
  // with enums.
};

static constexpr const char* kIRNodeStrings[] = {
#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME) #NAME,
// NOLINTNEXTLINE : build/include
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PX_CARNOT_IR_NODE
};

StatusOr<px::types::DataType> IRNodeTypeToDataType(IRNodeType type);
StatusOr<IRNodeType> DataTypeToIRNodeType(types::DataType type);

inline std::ostream& operator<<(std::ostream& out, IRNodeType node_type) {
  return out << kIRNodeStrings[static_cast<int64_t>(node_type)];
}

// TODO(philkuz) Remove this dependency by removing px prefix in the carnot func registry.
inline static constexpr char kPLFuncPrefix[] = "px";

/**
 * @brief Node class for the IR.
 *
 */
class IRNode {
 public:
  IRNode() = delete;
  virtual ~IRNode() = default;
  int64_t line() const { return line_; }
  int64_t col() const { return col_; }
  bool line_col_set() const { return line_col_set_; }

  virtual std::string DebugString() const;
  virtual bool IsOperator() const = 0;
  virtual bool IsExpression() const = 0;

  IRNodeType type() const { return type_; }
  std::string type_string() const { return TypeString(type()); }
  static std::string TypeString(const IRNodeType& node_type) {
    return kIRNodeStrings[static_cast<int64_t>(node_type)];
  }
  /**
   * @brief The graph that contains the node.
   *
   * @return IR*
   */
  IR* graph() const { return graph_; }

  /**
   * @brief Set the pointer to the graph.
   * The pointer is passed in by the Node factory of the graph
   * (see IR::MakeNode) so that we can add edges between this
   * object and any other objects created later on.
   *
   * @param graph : pointer to the graph object.
   */
  void set_graph(IR* graph) { graph_ = graph; }
  /**
   * @brief The id of the operator.
   *
   * @return int64_t
   */
  int64_t id() const { return id_; }

  /**
   * @brief The AST node that lead to this IRNode to be generated.
   *
   * @return pypa::AstPtr
   */
  pypa::AstPtr ast() const { return ast_; }

  /**
   * @brief Create an error that incorporates line, column of ir node into the error message.
   *
   * @param args: the arguments to the substitute that is called.
   * @return Status: Status with CompilerError context.
   */
  template <typename... Args>
  Status CreateIRNodeError(Args... args) const {
    auto msg = absl::Substitute(args...);
    compilerpb::CompilerErrorGroup context = LineColErrorPb(line(), col(), msg);
    return Status(statuspb::INVALID_ARGUMENT, msg,
                  std::make_unique<compilerpb::CompilerErrorGroup>(context));
  }
  /**
   * @brief Errors out if in debug mode, otherwise floats up an error.
   *
   * @return Status: status if not in debug mode.
   */
  template <typename... Args>
  Status DExitOrIRNodeError(Args... args) const {
    DCHECK(false) << absl::Substitute(args...);
    return CreateIRNodeError(args...);
  }

  /*
   * @brief Copy data from the input node into this node. All children classes need to implement
   * CopyFromNodeImpl. If a child class is itself a parent of other classes, then it must override
   * this class and call this method, followed by whatever operations that all of its child
   * classes must do during a CopyFromNode.
   *
   * @param node
   * @return Status
   */
  virtual Status CopyFromNode(const IRNode* node,
                              absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map);
  void SetLineCol(int64_t line, int64_t col);
  void SetLineCol(const pypa::AstPtr& ast);

  TypePtr resolved_type() { return resolved_type_; }
  const TypePtr resolved_type() const { return resolved_type_; }
  bool is_type_resolved() const { return resolved_type_ != nullptr; }
  Status SetResolvedType(TypePtr resolved_type) {
    resolved_type_ = resolved_type;
    return Status::OK();
  }
  virtual void ClearResolvedType() { resolved_type_ = nullptr; }

 protected:
  explicit IRNode(int64_t id, IRNodeType type) : type_(type), id_(id) {}
  virtual Status CopyFromNodeImpl(
      const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) = 0;

  IRNodeType type_;

 private:
  int64_t id_;
  // line and column where the parser read the data for this node.
  // used for highlighting errors in queries.
  int64_t line_;
  int64_t col_;
  IR* graph_;
  bool line_col_set_ = false;
  pypa::AstPtr ast_;
  TypePtr resolved_type_;
};

inline std::ostream& operator<<(std::ostream& out, IRNode* node) {
  return out << node->DebugString();
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
