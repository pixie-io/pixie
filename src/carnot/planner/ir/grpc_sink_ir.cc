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

#include "src/carnot/planner/ir/grpc_sink_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status GRPCSinkIR::CopyFromNodeImpl(const IRNode* node,
                                    absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const GRPCSinkIR* grpc_sink = static_cast<const GRPCSinkIR*>(node);
  sink_type_ = grpc_sink->sink_type_;
  destination_id_ = grpc_sink->destination_id_;
  destination_address_ = grpc_sink->destination_address_;
  destination_ssl_targetname_ = grpc_sink->destination_ssl_targetname_;
  name_ = grpc_sink->name_;
  out_columns_ = grpc_sink->out_columns_;
  return Status::OK();
}

Status GRPCSinkIR::ToProto(planpb::Operator* op) const {
  CHECK(has_output_table());
  auto pb = op->mutable_grpc_sink_op();
  op->set_op_type(planpb::GRPC_SINK_OPERATOR);
  pb->mutable_output_table()->set_table_name(name());
  pb->set_address(destination_address());
  pb->mutable_connection_options()->set_ssl_targetname(destination_ssl_targetname());

  DCHECK(is_type_resolved());
  for (const auto& [col_name, col_type] : *resolved_table_type()) {
    DCHECK(col_type->IsValueType());
    auto val_type = std::static_pointer_cast<ValueType>(col_type);
    pb->mutable_output_table()->add_column_names(col_name);
    pb->mutable_output_table()->add_column_types(val_type->data_type());
    pb->mutable_output_table()->add_column_semantic_types(val_type->semantic_type());
  }
  return Status::OK();
}

Status GRPCSinkIR::ToProto(planpb::Operator* op, int64_t agent_id) const {
  auto pb = op->mutable_grpc_sink_op();
  op->set_op_type(planpb::GRPC_SINK_OPERATOR);
  pb->set_address(destination_address());
  pb->mutable_connection_options()->set_ssl_targetname(destination_ssl_targetname());
  if (!agent_id_to_destination_id_.contains(agent_id)) {
    return CreateIRNodeError("No agent ID '$0' found in grpc sink '$1'", agent_id, DebugString());
  }
  pb->set_grpc_source_id(agent_id_to_destination_id_.find(agent_id)->second);
  return Status::OK();
}

Status GRPCSinkIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_EQ(1U, parent_types().size());
  // When out_columns_ is empty, the GRPCSink just copies the parent type.
  if (out_columns_.size() == 0) {
    PX_ASSIGN_OR_RETURN(auto type_ptr, OperatorIR::DefaultResolveType(parent_types()));
    return SetResolvedType(type_ptr);
  }
  auto parent_table_type = std::static_pointer_cast<TableType>(parent_types()[0]);
  auto table = TableType::Create();
  for (const auto& col_name : out_columns_) {
    PX_ASSIGN_OR_RETURN(auto col_type, parent_table_type->GetColumnType(col_name));
    table->AddColumn(col_name, col_type);
  }
  return SetResolvedType(table);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
