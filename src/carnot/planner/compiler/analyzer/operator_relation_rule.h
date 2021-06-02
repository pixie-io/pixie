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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class OperatorRelationRule : public Rule {
  /**
   * @brief OperatorRelationRule sets up relations for non-source operators.
   */
 public:
  explicit OperatorRelationRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> SetBlockingAgg(BlockingAggIR* agg_ir) const;
  StatusOr<bool> SetMap(MapIR* map_ir) const;
  StatusOr<bool> SetUnion(UnionIR* union_ir) const;
  StatusOr<bool> SetOldJoin(JoinIR* join_op) const;
  StatusOr<bool> SetMemorySink(MemorySinkIR* map_ir) const;
  StatusOr<bool> SetGRPCSink(GRPCSinkIR* map_ir) const;
  StatusOr<bool> SetRolling(RollingIR* rolling_ir) const;
  StatusOr<bool> SetOther(OperatorIR* op) const;

  /**
   * @brief Setup the output columns for a Join. This is done before we create the relation of a
   * Join in case it doesn't have output columns set yet, as is the case with the pandas merge
   * syntax.
   *
   * What this method produces
   * 1. The columns order is left df columns in order followed by the right df columns in order.
   * 2. Join columns are not considered any differently -> both join columns will appear in the
   * resulting dataframe according to the columns.
   * 3. Duplicated column names will be suffixed with the Join's suffix labels
   * - If the duplicated column names are not unique after the suffixing, then it errors out.
   * - This is actually slightly different behavior than in Pandas but I don't think we can
   * replicate the resulting ability to index two columns at once. It's probably better to error
   * out than allow it. Proof that this is a feature:
   * https://github.com/pandas-dev/pandas/pull/10639
   * 4. Will update the Join so that output columns are set _and have data type resolved_.
   *
   *
   * @param join_op the join operator to create output columns for.
   * @return Status
   */
  Status SetJoinOutputColumns(JoinIR* join_op) const;

  /**
   * @brief Create a Output Column IR nodes vector from the relations. Simply just copy the
   * columns from the parents.
   *
   * @param join_node the join node to error on in case of issues.
   * @param left_relation the left dataframe's relation.
   * @param right_relation the right dataframe's relation.
   * @param left_idx the index of the left dataframe, in case it was flipped
   * @param right_idx the index of the left dataframe, in case it was flipped
   * @return StatusOr<std::vector<ColumnIR*>> the columns as as vector or an error
   */
  StatusOr<std::vector<ColumnIR*>> CreateOutputColumnIRNodes(
      JoinIR* join_node, const table_store::schema::Relation& left_relation,
      const table_store::schema::Relation& right_relation) const;
  StatusOr<ColumnIR*> CreateOutputColumn(JoinIR* join_node, const std::string& col_name,
                                         int64_t parent_idx,
                                         const table_store::schema::Relation& relation) const;
  void ReplaceDuplicateNames(std::vector<std::string>* column_names, const std::string& dup_name,
                             const std::string& left_column, const std::string& right_column) const;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
