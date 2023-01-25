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
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The Join Operator plan node.
 *
 */
class JoinIR : public OperatorIR {
 public:
  enum class JoinType { kLeft, kRight, kOuter, kInner };

  JoinIR() = delete;
  explicit JoinIR(int64_t id) : OperatorIR(id, IRNodeType::kJoin) {}

  bool IsBlocking() const override { return true; }

  Status ToProto(planpb::Operator*) const override;

  /**
   * @brief JoinIR init to directly initialize the operator.
   *
   * @param parents
   * @param how_type
   * @param left_on_cols
   * @param right_on_cols
   * @param suffix_strs
   * @return Status
   */
  Status Init(const std::vector<OperatorIR*>& parents, const std::string& how_type,
              const std::vector<ColumnIR*>& left_on_cols,
              const std::vector<ColumnIR*>& right_on_cols,
              const std::vector<std::string>& suffix_strs);
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  JoinType join_type() const { return join_type_; }
  const std::vector<ColumnIR*>& output_columns() const { return output_columns_; }
  const std::vector<std::string>& column_names() const { return column_names_; }
  Status SetJoinType(JoinType join_type) {
    join_type_ = join_type;
    return Status::OK();
  }
  Status SetJoinType(const std::string& join_type) {
    PX_ASSIGN_OR_RETURN(join_type_, GetJoinEnum(join_type));
    if (join_type_ == JoinType::kRight) {
      specified_as_right_ = true;
    }
    return Status::OK();
  }

  const std::vector<ColumnIR*>& left_on_columns() const { return left_on_columns_; }
  const std::vector<ColumnIR*>& right_on_columns() const { return right_on_columns_; }
  const std::vector<std::string>& suffix_strs() const { return suffix_strs_; }
  void SetSuffixStrs(const std::vector<std::string>& suffix_strs) { suffix_strs_ = suffix_strs; }
  Status SetOutputColumns(const std::vector<std::string>& column_names,
                          const std::vector<ColumnIR*>& columns);
  bool specified_as_right() const { return specified_as_right_; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

  const std::tuple<std::shared_ptr<TableType>, std::shared_ptr<TableType>> left_right_table_types()
      const {
    auto left_idx = 0;
    auto right_idx = 1;
    if (specified_as_right()) {
      left_idx = 1;
      right_idx = 0;
    }
    auto left_type = parents()[left_idx]->resolved_table_type();
    auto right_type = parents()[right_idx]->resolved_table_type();
    return std::make_tuple(left_type, right_type);
  }

  const std::tuple<std::string, std::string> left_right_suffixs() const {
    auto left_idx = 0;
    auto right_idx = 1;
    if (specified_as_right()) {
      left_idx = 1;
      right_idx = 0;
    }
    return std::make_tuple(suffix_strs_[left_idx], suffix_strs_[right_idx]);
  }

  Status ResolveType(CompilerState* compiler_state);

  Status UpdateOpAfterParentTypesResolvedImpl() override;

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& kept_columns) override;

 private:
  /**
   * @brief Converts the string type to JoinIR::JoinType or errors out if it doesn't exist.
   *
   * @param join_type string representation of the join.
   * @return StatusOr<planpb::JoinOperator::JoinType> the join enum or an error if not found.
   */
  StatusOr<JoinType> GetJoinEnum(const std::string& join_type) const;

  /**
   * @brief Get the Protobuf JoinType for the JoinIR::JoinType
   *
   * @param join_type
   * @return planpb::JoinOperator::JoinType
   */
  static planpb::JoinOperator::JoinType GetPbJoinEnum(JoinType join_type);

  Status SetJoinColumns(const std::vector<ColumnIR*>& left_columns,
                        const std::vector<ColumnIR*>& right_columns);

  // Join type
  JoinType join_type_;
  // Whether or not the join key columns have been set.
  bool key_columns_set_ = false;
  // The columns that are output by this join operator.
  std::vector<ColumnIR*> output_columns_;
  // The column names to set.
  std::vector<std::string> column_names_;
  // The columns we join from the left parent.
  std::vector<ColumnIR*> left_on_columns_;
  // The columns we join from the right parent.
  std::vector<ColumnIR*> right_on_columns_;
  // The suffixes to add to the left columns and to the right columns.
  std::vector<std::string> suffix_strs_;

  // Whether this join was originally specified as a right join.
  // Used because we transform left joins into right joins but need to do some back transform.
  bool specified_as_right_ = false;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
