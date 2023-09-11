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

#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
/**
 * @brief PartialOperatorMgr is the interface to managed splitting Blocking Operators into portions
 * that can run local to the original data and portions that merge those local resutls into a single
 * result.
 *
 * For aggregates, an implementation will have a prepare statement that creates a partial aggregate,
 * then the Merge will be the Finalize Aggregate.
 */
class PartialOperatorMgr : public NotCopyable {
 public:
  virtual ~PartialOperatorMgr() = default;
  /**
   * @brief Returns true if this Mgr can split up the pasesd in operator.
   *
   * @param op
   * @return true
   * @return false
   */
  virtual bool Matches(OperatorIR* op) const = 0;
  /**
   * @brief Create the Prepare Operator that does the "local" portion of the operator. For
   * aggregates, this would be the partial aggregate.
   *
   * @param plan
   * @param op
   * @return OperatorIR*
   */
  virtual StatusOr<OperatorIR*> CreatePrepareOperator(IR* plan, OperatorIR* op) const = 0;

  /**
   * @brief Create the Merge version of this operator. This takes in results of PrepareOperators
   * from several different sources and merges them to match the expected output of the original
   * operator.
   *
   * @param plan
   * @param new_parent
   * @param op
   * @return OperatorIR*
   */
  virtual StatusOr<OperatorIR*> CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                                    OperatorIR* op) const = 0;
};

/**
 * @brief LimitOperatorMgr manages splitting limits over the boundary. Prepare Limits are the same
 * as the Merge operators, but it's a nice optimization because it removes a very unnecessary
 * network transfer.
 *
 */
class LimitOperatorMgr : public PartialOperatorMgr {
 public:
  bool Matches(OperatorIR* op) const override {
    if (!Match(op, Limit())) {
      return false;
    }
    return !static_cast<LimitIR*>(op)->pem_only();
  }
  StatusOr<OperatorIR*> CreatePrepareOperator(IR* plan, OperatorIR* op) const override;
  StatusOr<OperatorIR*> CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                            OperatorIR* op) const override;
};

/**
 * @brief AggOperatorMgr manages splitting aggregates into partial aggregate and the merging node
 * over a network boundary.
 */
class AggOperatorMgr : public PartialOperatorMgr {
 public:
  bool Matches(OperatorIR* op) const override {
    if (!Match(op, BlockingAgg())) {
      return false;
    }
    BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
    for (const auto& col_expr : agg->aggregate_expressions()) {
      if (!Match(col_expr.node, PartialUDA())) {
        return false;
      }
    }
    return true;
  }
  StatusOr<OperatorIR*> CreatePrepareOperator(IR* plan, OperatorIR* op) const override;
  StatusOr<OperatorIR*> CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                            OperatorIR* op) const override;
};
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
