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

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/distributed/grpc_source_conversion.h"
#include "src/carnot/planner/ir/empty_source_ir.h"
#include "src/carnot/planner/ir/grpc_source_group_ir.h"
#include "src/carnot/planner/ir/grpc_source_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/union_ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using table_store::schema::Relation;

StatusOr<bool> GRPCSourceGroupConversionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GRPCSourceGroup())) {
    return ExpandGRPCSourceGroup(static_cast<GRPCSourceGroupIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> GRPCSourceGroupConversionRule::ExpandGRPCSourceGroup(GRPCSourceGroupIR* group_ir) {
  // Get the new parent.
  PX_ASSIGN_OR_RETURN(OperatorIR * new_parent, ConvertGRPCSourceGroup(group_ir));
  for (const auto child : group_ir->Children()) {
    // Replace the child node's parent with the new parent.
    PX_RETURN_IF_ERROR(child->ReplaceParent(group_ir, new_parent));
  }
  IR* graph = group_ir->graph();
  // Remove the old group_ir from the graph.
  PX_RETURN_IF_ERROR(graph->DeleteNode(group_ir->id()));
  return true;
}

StatusOr<GRPCSourceIR*> GRPCSourceGroupConversionRule::CreateGRPCSource(
    GRPCSourceGroupIR* group_ir) {
  DCHECK(group_ir->is_type_resolved());
  IR* graph = group_ir->graph();
  return graph->CreateNode<GRPCSourceIR>(group_ir->ast(), group_ir->resolved_type());
}

// Have to get rid of this function. Instead, need to associate (agent_id, sink_id) ->
// source_id/destination_id.
Status UpdateSink(GRPCSourceIR* source, GRPCSinkIR* sink, int64_t agent_id) {
  sink->AddDestinationIDMap(source->id(), agent_id);
  return Status::OK();
}

StatusOr<OperatorIR*> GRPCSourceGroupConversionRule::ConvertGRPCSourceGroup(
    GRPCSourceGroupIR* group_ir) {
  auto ir_graph = group_ir->graph();
  auto sinks = group_ir->dependent_sinks();

  if (sinks.size() == 0) {
    PX_ASSIGN_OR_RETURN(
        EmptySourceIR * empty_source,
        ir_graph->CreateNode<EmptySourceIR>(group_ir->ast(), group_ir->resolved_type()));
    return empty_source;
  }

  // Don't add an unnecessary union node if there is only one sink.
  if (sinks.size() == 1 && sinks[0].second.size() == 1) {
    PX_ASSIGN_OR_RETURN(auto new_grpc_source, CreateGRPCSource(group_ir));
    PX_RETURN_IF_ERROR(UpdateSink(new_grpc_source, sinks[0].first, *(sinks[0].second.begin())));
    return new_grpc_source;
  }

  std::vector<OperatorIR*> grpc_sources;
  for (const auto& sink : sinks) {
    DCHECK_GE(sinks[0].second.size(), 1U);
    for (int64_t agent_id : sink.second) {
      PX_ASSIGN_OR_RETURN(GRPCSourceIR * new_grpc_source, CreateGRPCSource(group_ir));
      PX_RETURN_IF_ERROR(UpdateSink(new_grpc_source, sink.first, agent_id));
      grpc_sources.push_back(new_grpc_source);
    }
  }

  PX_ASSIGN_OR_RETURN(UnionIR * union_op,
                      ir_graph->CreateNode<UnionIR>(group_ir->ast(), grpc_sources));
  PX_RETURN_IF_ERROR(union_op->SetResolvedType(grpc_sources[0]->resolved_type()));
  PX_RETURN_IF_ERROR(union_op->SetDefaultColumnMapping());
  return union_op;
}

StatusOr<bool> MergeSameNodeGRPCBridgeRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, InternalGRPCSink())) {
    return false;
  }
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(ir_node);
  DCHECK(grpc_sink->agent_id_to_destination_id().contains(current_agent_id_))
      << "Expected the grpc sink to contain this current agent ID as a a target";
  int64_t dest_id = grpc_sink->agent_id_to_destination_id().at(current_agent_id_);
  auto node = grpc_sink->graph()->Get(dest_id);
  if (!Match(node, GRPCSource())) {
    return node->CreateIRNodeError("Expected node to be a 'GRPCSource', but recieved a '$0'",
                                   node->DebugString());
  }
  auto grpc_sink_parent = grpc_sink->parents()[0];
  PX_RETURN_IF_ERROR(grpc_sink->RemoveParent(grpc_sink_parent));
  auto grpc_source_to_replace = static_cast<GRPCSourceIR*>(node);
  for (OperatorIR* child : grpc_source_to_replace->Children()) {
    PX_RETURN_IF_ERROR(child->ReplaceParent(grpc_source_to_replace, grpc_sink_parent));
  }
  IR* graph = grpc_source_to_replace->graph();
  PX_RETURN_IF_ERROR(graph->DeleteNode(grpc_sink->id()));
  PX_RETURN_IF_ERROR(graph->DeleteNode(grpc_source_to_replace->id()));

  return true;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
