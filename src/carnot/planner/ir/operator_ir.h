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
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

// Forward declaration for types that are used in OperatorIR. They are declared later in this
// header.
class ColumnIR;

/**
 * @brief Node class for the operator
 *
 */
class OperatorIR : public IRNode {
 public:
  OperatorIR() = delete;
  bool IsOperator() const override { return true; }
  bool IsExpression() const override { return false; }
  bool HasParents() const { return parents_.size() != 0; }
  bool IsChildOf(OperatorIR* parent) {
    return std::find(parents_.begin(), parents_.end(), parent) != parents_.end();
  }
  const std::vector<OperatorIR*>& parents() const { return parents_; }
  Status AddParent(OperatorIR* node);
  Status RemoveParent(OperatorIR* op);

  /**
   * @brief Replaces the old_parent with the new parent. Errors out if the old_parent isn't actually
   * a parent.
   *
   * @param old_parent: operator's parent that will be replaced.
   * @param new_parent: the new operator to replace old_parent with.
   * @return Status: Error if old_parent not an actualy parent.
   */
  Status ReplaceParent(OperatorIR* old_parent, OperatorIR* new_parent);

  virtual Status ToProto(planpb::Operator*) const = 0;

  std::string ParentsDebugString();
  std::string ChildrenDebugString();
  Status CopyParentsFrom(const OperatorIR* og_op);

  /**
   * @brief Override of CopyFromNode that adds special handling for Operators.
   */
  Status CopyFromNode(const IRNode* node,
                      absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  /**
   * @brief Removes any output columns that are not needed to produce those listed in
   * 'output_colnames'. It expects that the relation has already been set. It is only intended to be
   * run on nodes that make it to the final plan (so a node like Drop would not have an
   * implementation).
   *
   */
  Status PruneOutputColumnsTo(const absl::flat_hash_set<std::string>& output_colnames);

  /**
   * @brief Returns the Operator children of this node.
   *
   * @return std::vector<OperatorIR*>: the vector of operator children of this node.
   */
  std::vector<OperatorIR*> Children() const;

  virtual bool IsBlocking() const { return false; }

  virtual bool IsSource() const { return false; }

  /**
   * @brief For each parent (ordered by index), returns the required column names to execute this
   * operator.
   *
   */
  virtual StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const = 0;

  static bool NodeMatches(IRNode* input);
  // This is different from type_string() which assumes a leaf node IRNode type on a specific
  // object. Sometimes when producing error messages, we want to print out that something did not
  // match an "Operator" generally, when any "leaf" type of Operator would have been a match.
  static std::string class_type_string() { return "Operator"; }

  static StatusOr<TypePtr> DefaultResolveType(const std::vector<TypePtr>& parent_types);

  const std::vector<TypePtr>& parent_types() const {
    DCHECK(parent_types_set_);
    return parent_types_;
  }

  void PullParentTypes() {
    DCHECK(!parent_types_set_);
    for (const auto& parent : parents()) {
      DCHECK(parent->is_type_resolved());
      parent_types_.push_back(parent->resolved_type());
    }
    parent_types_set_ = true;
  }

  // Some operators don't need to resolve type. For example, some ops only occur at
  // the distributed level eg GRPCSource, and type information shouldn't be needed at the
  // distributed stage.
  static constexpr bool FailOnResolveType() { return false; }

  void ClearResolvedType() override {
    IRNode::ClearResolvedType();
    parent_types_set_ = false;
    parent_types_.clear();
  }

  std::shared_ptr<TableType> resolved_table_type() const {
    return std::static_pointer_cast<TableType>(resolved_type());
  }

  // Some operators must do things (eg. Join's have to work out output columns, map's have to
  // deal with keep_input_columns, etc) after their parents' types have been resolved.
  // This function gets called after all the parent types are resolved but before ResolveType is
  // called on the operator.
  Status UpdateOpAfterParentTypesResolved() {
    PX_RETURN_IF_ERROR(UpdateOpAfterParentTypesResolvedImpl());
    return Status::OK();
  }

 protected:
  explicit OperatorIR(int64_t id, IRNodeType type) : IRNode(id, type) {}

  /**
   * @brief Support for operators that have the same parent multiple times, like a self-join.
   */
  StatusOr<std::vector<OperatorIR*>> HandleDuplicateParents(
      const std::vector<OperatorIR*>& parents);

  /**
   * @brief Impl for PruneOutputColumnsTo. Returns the set of column names that remain after the
   * pruning. These are distinct from 'output_colnames' because some columns may not be listed in
   * that set but cannot be pruned from the operator (such as a group by in a BlockingAgg).
   *
   */
  virtual StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_colnames) = 0;

  virtual Status UpdateOpAfterParentTypesResolvedImpl() { return Status::OK(); }

 private:
  std::vector<OperatorIR*> parents_;
  std::vector<TypePtr> parent_types_;
  bool parent_types_set_ = false;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
