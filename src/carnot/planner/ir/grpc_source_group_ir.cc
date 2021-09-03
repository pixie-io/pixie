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

#include "src/carnot/planner/ir/grpc_source_group_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status GRPCSourceGroupIR::CopyFromNodeImpl(const IRNode* node,
                                           absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const GRPCSourceGroupIR* grpc_source_group = static_cast<const GRPCSourceGroupIR*>(node);
  source_id_ = grpc_source_group->source_id_;
  grpc_address_ = grpc_source_group->grpc_address_;
  if (grpc_source_group->dependent_sinks_.size()) {
    return error::Unimplemented("Cannot clone GRPCSourceGroupIR with dependent_sinks_");
  }
  return Status::OK();
}

Status GRPCSourceGroupIR::ToProto(planpb::Operator* op) const {
  // Note this is more for testing.
  auto pb = op->mutable_grpc_source_op();
  op->set_op_type(planpb::GRPC_SOURCE_OPERATOR);

  for (const auto& [col_name, col_type] : *resolved_table_type()) {
    DCHECK(col_type->IsValueType());
    auto val_type = std::static_pointer_cast<ValueType>(col_type);
    pb->add_column_types(val_type->data_type());
    pb->add_column_names(col_name);
  }

  return Status::OK();
}

Status GRPCSourceGroupIR::AddGRPCSink(GRPCSinkIR* sink_op,
                                      const absl::flat_hash_set<int64_t>& agents) {
  if (sink_op->destination_id() != source_id_) {
    return DExitOrIRNodeError("Source id $0 and destination id $1 aren't equal.",
                              sink_op->destination_id(), source_id_);
  }
  if (!GRPCAddressSet()) {
    return DExitOrIRNodeError("$0 doesn't have a physical agent associated with it.",
                              DebugString());
  }
  sink_op->SetDestinationAddress(grpc_address_);
  sink_op->SetDestinationSSLTargetName(ssl_targetname_);
  dependent_sinks_.emplace_back(sink_op, agents);
  return Status::OK();
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
