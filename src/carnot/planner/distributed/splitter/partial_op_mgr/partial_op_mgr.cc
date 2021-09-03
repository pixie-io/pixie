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

#include "src/carnot/planner/distributed/splitter/partial_op_mgr/partial_op_mgr.h"

#include <string>
#include <utility>
#include <vector>

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<OperatorIR*> LimitOperatorMgr::CreatePrepareOperator(IR* plan, OperatorIR* op) const {
  DCHECK(Matches(op));
  LimitIR* limit = static_cast<LimitIR*>(op);
  PL_ASSIGN_OR_RETURN(LimitIR * new_limit, plan->CopyNode(limit));
  PL_RETURN_IF_ERROR(new_limit->CopyParentsFrom(limit));
  return new_limit;
}

StatusOr<OperatorIR*> LimitOperatorMgr::CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                                            OperatorIR* op) const {
  DCHECK(Matches(op));
  LimitIR* limit = static_cast<LimitIR*>(op);
  PL_ASSIGN_OR_RETURN(LimitIR * new_limit, plan->CopyNode(limit));
  PL_RETURN_IF_ERROR(new_limit->AddParent(new_parent));
  return new_limit;
}

StatusOr<OperatorIR*> AggOperatorMgr::CreatePrepareOperator(IR* plan, OperatorIR* op) const {
  DCHECK(Matches(op));
  BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
  PL_ASSIGN_OR_RETURN(BlockingAggIR * new_agg, plan->CopyNode(agg));
  PL_RETURN_IF_ERROR(new_agg->CopyParentsFrom(agg));
  new_agg->SetPartialAgg(true);
  new_agg->SetFinalizeResults(false);

  auto new_type = TableType::Create();
  for (ColumnIR* group : agg->groups()) {
    DCHECK(group->is_type_resolved());
    new_type->AddColumn(group->col_name(), group->resolved_type());
  }

  // Add column for the serialized expression
  new_type->AddColumn("serialized_expressions", ValueType::Create(types::STRING, types::ST_NONE));
  PL_RETURN_IF_ERROR(new_agg->SetResolvedType(new_type));

  DCHECK(Match(new_agg, PartialAgg()));
  return new_agg;
}

StatusOr<OperatorIR*> AggOperatorMgr::CreateMergeOperator(IR* plan, OperatorIR* new_parent,
                                                          OperatorIR* op) const {
  DCHECK(Matches(op));
  BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
  PL_ASSIGN_OR_RETURN(BlockingAggIR * new_agg, plan->CopyNode(agg));
  PL_RETURN_IF_ERROR(new_agg->AddParent(new_parent));
  planpb::Operator pb;
  PL_RETURN_IF_ERROR(agg->ToProto(&pb));
  new_agg->SetPreSplitProto(pb.agg_op());

  new_agg->SetPartialAgg(false);
  new_agg->SetFinalizeResults(true);
  DCHECK(Match(new_agg, FinalizeAgg()));
  return new_agg;
}
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
