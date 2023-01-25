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

#include "src/carnot/exec/map_node.h"

#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string MapNode::DebugStringImpl() {
  return absl::Substitute("Exec::MapNode<$0>", evaluator_->DebugString());
}

Status MapNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::MAP_OPERATOR);
  const auto* map_plan_node = static_cast<const plan::MapOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MapOperator>(*map_plan_node);
  return Status::OK();
}
Status MapNode::PrepareImpl(ExecState* exec_state) {
  function_ctx_ = exec_state->CreateFunctionContext();
  evaluator_ = ScalarExpressionEvaluator::Create(
      plan_node_->expressions(), ScalarExpressionEvaluatorType::kArrowNative, function_ctx_.get());
  return Status::OK();
}

Status MapNode::OpenImpl(ExecState* exec_state) {
  PX_RETURN_IF_ERROR(evaluator_->Open(exec_state));
  return Status::OK();
}

Status MapNode::CloseImpl(ExecState* exec_state) {
  PX_RETURN_IF_ERROR(evaluator_->Close(exec_state));
  return Status::OK();
}
Status MapNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t) {
  RowBatch output_rb(*output_descriptor_, rb.num_rows());
  PX_RETURN_IF_ERROR(evaluator_->Evaluate(exec_state, rb, &output_rb));
  output_rb.set_eow(rb.eow());
  output_rb.set_eos(rb.eos());
  PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, output_rb));
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
