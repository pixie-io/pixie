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

#include <absl/container/flat_hash_set.h>
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/column_expression.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/group_acceptor_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {
/**
 * @brief The BlockingAggIR is the IR representation for the Agg operator.
 * GroupBy groups() and Aggregate columns according to aggregate_expressions().
 */
class BlockingAggIR : public GroupAcceptorIR {
 public:
  BlockingAggIR() = delete;
  explicit BlockingAggIR(int64_t id) : GroupAcceptorIR(id, IRNodeType::kBlockingAgg) {}

  Status Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups,
              const ColExpressionVector& agg_expr);

  Status ToProto(planpb::Operator*) const override;
  Status EvaluateAggregateExpression(planpb::AggregateExpression* expr,
                                     const ExpressionIR& ir_node) const;

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  inline bool IsBlocking() const override { return true; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

  Status SetAggExprs(const ColExpressionVector& agg_expr);

  Status ResolveType(CompilerState* compiler_state);

  ColExpressionVector aggregate_expressions() const { return aggregate_expressions_; }

  void SetFinalizeResults(bool finalize_results) { finalize_results_ = finalize_results; }

  void SetPartialAgg(bool partial_agg) { partial_agg_ = partial_agg; }

  bool partial_agg() const { return partial_agg_; }
  bool finalize_results() const { return finalize_results_; }
  void SetPreSplitProto(const planpb::AggregateOperator& pre_split_proto) {
    pre_split_proto_ = pre_split_proto;
  }
  std::string DebugString() const override;

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_colnames) override;

 private:
  // The map from value_names to values.
  ColExpressionVector aggregate_expressions_;
  // Whether this performs a partial aggregate.
  bool partial_agg_ = true;
  // Whether this finalizes the result of a partial aggregate.
  bool finalize_results_ = true;
  planpb::AggregateOperator pre_split_proto_;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
