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

#include "src/carnot/exec/limit_node.h"

#include <arrow/array.h>
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

std::string LimitNode::DebugStringImpl() {
  return absl::Substitute("Exec::LimitNode<$0>", plan_node_->DebugString());
}

Status LimitNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::LIMIT_OPERATOR);
  const auto* limit_plan_node = static_cast<const plan::LimitOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::LimitOperator>(*limit_plan_node);

  return Status::OK();
}
Status LimitNode::PrepareImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status LimitNode::OpenImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status LimitNode::CloseImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status LimitNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t) {
  int64_t record_limit = plan_node_->record_limit();
  // We need to send over a slice of the input data.
  int64_t remainder_records = record_limit - records_processed_;

  if (remainder_records < 0) {
    remainder_records = 0;
  }

  if (limit_reached_) {
    return Status::OK();
  }

  // Check if the entire row batch will fit.
  if (remainder_records > rb.num_rows()) {
    RowBatch output_rb(*output_descriptor_, rb.num_rows());
    DCHECK_EQ(output_descriptor_->size(), plan_node_->selected_cols().size());
    // If so we just need to convert to output descriptor and transfer it.
    for (int64_t input_col_idx : plan_node_->selected_cols()) {
      PX_RETURN_IF_ERROR(output_rb.AddColumn(rb.ColumnAt(input_col_idx)));
    }
    records_processed_ += rb.num_rows();
    output_rb.set_eos(rb.eos());
    output_rb.set_eow(rb.eow());
    return SendRowBatchToChildren(exec_state, output_rb);
  }

  RowBatch output_rb(*output_descriptor_, remainder_records);
  DCHECK_EQ(output_descriptor_->size(), plan_node_->selected_cols().size());
  for (int64_t input_col_idx : plan_node_->selected_cols()) {
    auto col = rb.ColumnAt(input_col_idx);
    PX_RETURN_IF_ERROR(output_rb.AddColumn(col->Slice(0, remainder_records)));
  }
  output_rb.set_eow(true);
  output_rb.set_eos(true);
  records_processed_ += remainder_records;
  limit_reached_ = true;

  // Terminate execution.
  for (const auto src_id : plan_node_->abortable_srcs()) {
    exec_state->StopSource(src_id);
  }

  return SendRowBatchToChildren(exec_state, output_rb);
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
