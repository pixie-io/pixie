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

#include "src/carnot/exec/memory_sink_node.h"

#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::Table;
using table_store::schema::Relation;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string MemorySinkNode::DebugStringImpl() {
  return absl::Substitute("Exec::MemorySinkNode: {name: $0, output: $1}", plan_node_->TableName(),
                          input_descriptor_->DebugString());
}

Status MemorySinkNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::MEMORY_SINK_OPERATOR);
  const auto* sink_plan_node = static_cast<const plan::MemorySinkOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MemorySinkOperator>(*sink_plan_node);
  if (input_descriptors_.size() != 1) {
    return error::InvalidArgument("MemorySink operator expects a single input relation, got $0",
                                  input_descriptors_.size());
  }
  input_descriptor_ = std::make_unique<table_store::schema::RowDescriptor>(input_descriptors_[0]);

  return Status::OK();
}

Status MemorySinkNode::PrepareImpl(ExecState* exec_state_) {
  // Create Table.
  std::vector<std::string> col_names;
  for (size_t i = 0; i < input_descriptor_->size(); i++) {
    col_names.push_back(plan_node_->ColumnName(i));
  }

  table_ = Table::Create(TableName(), Relation(input_descriptor_->types(), col_names));
  exec_state_->table_store()->AddTable(plan_node_->TableName(), table_);

  return Status::OK();
}

Status MemorySinkNode::OpenImpl(ExecState*) { return Status::OK(); }

Status MemorySinkNode::CloseImpl(ExecState*) { return Status::OK(); }

Status MemorySinkNode::ConsumeNextImpl(ExecState*, const RowBatch& rb, size_t) {
  DCHECK_EQ(0U, children().size());
  if (rb.num_rows() > 0 || (rb.eow() || rb.eos())) {
    PX_RETURN_IF_ERROR(table_->WriteRowBatch(rb));
  }
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
