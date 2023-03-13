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

#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief ColumnIR wraps around columns in the plan.
 *
 * Columns have two important relationships that can easily get confused with one another.
 *
 * _Relationships_:
 * 1. A column is contained by an Operator.
 * 2. A column has an Operator that it references.
 *
 * The first relationship applies to Operators that utilize expressions (ie Map, Agg). Any
 * ColumnIR that is a progeny of an expression in an Operator means the Operator contains that
 * ColumnIR.
 *
 * The second relationship is between the ColumnIR and the parent of the Containing Operator (in
 * relationship #1) that the ColumnIR refers to. A ColumnIR that references an operator means it's
 * value is determined by the Operator and subsequently the Operator's output relation must
 * contain the column name used by this column.
 *
 * Because we are constantly shuffling parents of each Operator, ColumnIR's
 * methods are meant to be decoupled from the Operator that it references. Instead, ColumnIR
 * finds the referenced operator by first getting the containing operator, then grabbing the
 * saved index of that containing operator to point to a parent of that operator. This is under the
 * assumption that once we initialize an operator, the number of parents it has never changes.
 *
 * To get the Referenced Operator for a column, the Column accesses its saved index of the
 * Containing Operator's parents vector.
 *
 */
class ColumnIR : public ExpressionIR {
 public:
  ColumnIR() = delete;
  ColumnIR(int64_t id, const ExpressionIR::Annotations& annotations)
      : ExpressionIR(id, IRNodeType::kColumn, annotations) {}
  explicit ColumnIR(int64_t id) : ColumnIR(id, ExpressionIR::Annotations()) {}

  /**
   * @brief Creates a new column that references the passed in operator.
   *
   * @param col_name: The name of the column.
   * @param parent_op_idx: The index of the parent operator that this column references.
   * @return Status: error container.
   */
  Status Init(const std::string& col_name, int64_t parent_op_idx);

  std::string col_name() const { return col_name_; }

  bool IsColumn() const override { return true; }

  /**
   * @brief The operators containing this column. There can be multiple, but all of these operators
   * should share a common parent (ReferencedOperators).
   *
   * @return OperatorIR*: the operators containing this column
   */
  StatusOr<std::vector<OperatorIR*>> ContainingOperators() const;

  /**
   * @brief The operator that this column references. This should be the parent of the operator that
   * contains this column.
   *
   * @return OperatorIR*: the operator this column references.
   */
  StatusOr<OperatorIR*> ReferencedOperator() const;

  // TODO(philkuz) (PL-826): redo DebugString to have a DebugStringImpl instead of override.
  // NOLINTNEXTLINE(readability/inheritance)
  virtual std::string DebugString() const override;
  StatusOr<int64_t> ReferenceID() const {
    PX_ASSIGN_OR_RETURN(OperatorIR * referenced_op, ReferencedOperator());
    return referenced_op->id();
  }
  types::DataType EvaluatedDataType() const override {
    if (!is_type_resolved()) {
      return types::DataType::DATA_TYPE_UNKNOWN;
    }
    return resolved_value_type()->data_type();
  }
  bool IsDataTypeEvaluated() const override { return is_type_resolved(); }

  StatusOr<int64_t> GetColumnIndex() const;

  int64_t container_op_parent_idx() const { return container_op_parent_idx_; }
  bool container_op_parent_idx_set() const { return container_op_parent_idx_set_; }

  /**
   * @brief Override CopyFromNode to make sure all Column classes save the column attributes.
   */
  Status CopyFromNode(const IRNode* node,
                      absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;
  void SetContainingOperatorParentIdx(int64_t container_op_parent_idx);

  /**
   * @brief ToProto method for the Column class and derived classes.
   *
   * @param expr
   * @return error if we can't convert the Column into an expression.
   */
  Status ToProto(planpb::ScalarExpression* expr) const override;

  /**
   * @brief ToProto method that takes in a column message instead of a ScalarExpression.
   *
   * @param column_pb
   * @return Status
   */
  Status ToProto(planpb::Column* column_pb) const;

  bool Equals(ExpressionIR* expr) const override {
    if (!expr->IsColumn()) {
      return false;
    }
    auto col = static_cast<ColumnIR*>(expr);
    return col->col_name() == col_name() && (col->IsDataTypeEvaluated() == IsDataTypeEvaluated()) &&
           (!IsDataTypeEvaluated() || (col->EvaluatedDataType() == EvaluatedDataType()));
  }

  // Note: This should be used carefully, only when the column has just been created by a clone
  // and ideally has a single parent. Otherwise other dependents on this column may end up with
  // this update, when they actually wanted the original column name.
  void UpdateColumnName(const std::string& col_name) {
    DCHECK(col_name_set_);
    col_name_ = col_name;
  }

  Status ResolveType(CompilerState* compiler_state, const std::vector<TypePtr>& parent_types);
  static bool NodeMatches(IRNode* input);

 protected:
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;
  /**
   * @brief Optional protected constructor for children types.
   */
  ColumnIR(int64_t id, IRNodeType type, const ExpressionIR::Annotations& annotations)
      : ExpressionIR(id, type, annotations) {}
  ColumnIR(int64_t id, IRNodeType type) : ColumnIR(id, type, ExpressionIR::Annotations()) {}

  /**
   * @brief Point to the index of the Containing operator's parents that this column references.
   *
   * @param container_op_parent_idx: the index of the container_op.
   */

  void SetColumnName(const std::string& col_name) {
    col_name_ = col_name;
    col_name_set_ = true;
  }

 private:
  std::string col_name_;
  bool col_name_set_ = false;

  int64_t container_op_parent_idx_ = -1;
  bool container_op_parent_idx_set_ = false;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
