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

#include "src/carnot/exec/grpc_router.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <grpcpp/grpcpp.h>

#include "src/carnot/exec/grpc_source_node.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"

namespace px {
namespace carnot {
namespace exec {

Status GRPCRouter::EnqueueRowBatch(sole::uuid query_id,
                                   std::unique_ptr<carnotpb::TransferResultChunkRequest> req) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];

  if (!req->has_query_result() || !req->query_result().has_row_batch() ||
      req->query_result().destination_case() !=
          carnotpb::TransferResultChunkRequest_SinkResult::DestinationCase::kGrpcSourceId) {
    return error::Internal(
        "GRPCRouter::EnqueueRowBatch expected TransferResultChunkRequest to contain a row batch "
        "with a GPRC source ID.");
  }

  SourceNodeTracker& snt = query_map.source_node_trackers[req->query_result().grpc_source_id()];
  absl::base_internal::SpinLockHolder snt_lock(&snt.node_lock);
  // It's possible that we see row batches before we have gotten information about the query. To
  // solve this race, We store a backlog of all the pending batches.
  if (snt.source_node == nullptr) {
    snt.response_backlog.emplace_back(std::move(req));
    return Status::OK();
  }
  PL_RETURN_IF_ERROR(snt.source_node->EnqueueRowBatch(std::move(req)));
  query_map.restart_execution_func_();
  return Status::OK();
}

Status GRPCRouter::MarkResultStreamInitiated(sole::uuid query_id, int64_t source_id) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];

  SourceNodeTracker& snt = query_map.source_node_trackers[source_id];
  absl::base_internal::SpinLockHolder snt_lock(&snt.node_lock);
  // It's possible that we see row batches before we have gotten information about the query. To
  // solve this race, We store a backlog of all the pending batches.
  if (snt.source_node == nullptr) {
    DCHECK(!snt.connection_initiated_by_sink);
    snt.connection_initiated_by_sink = true;
    return Status::OK();
  }
  DCHECK(!snt.source_node->upstream_initiated_connection());
  snt.source_node->set_upstream_initiated_connection();
  return Status::OK();
}

Status GRPCRouter::MarkResultStreamClosed(sole::uuid query_id, int64_t source_id) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];

  SourceNodeTracker& snt = query_map.source_node_trackers[source_id];
  absl::base_internal::SpinLockHolder snt_lock(&snt.node_lock);
  // It's possible that we see row batches before we have gotten information about the query. To
  // solve this race, We store a backlog of all the pending batches.
  if (snt.source_node == nullptr) {
    DCHECK(!snt.connection_closed_by_sink);
    snt.connection_closed_by_sink = true;
    return Status::OK();
  }
  DCHECK(!snt.source_node->upstream_closed_connection());
  snt.source_node->set_upstream_closed_connection();
  return Status::OK();
}

// For all inbound result streams, we want to register the context of the stream,
// so that if the query gets cancelled, then we can make sure to also cancel the corresponding
// TransferResultChunk streams for that query.
void GRPCRouter::RegisterResultStreamContext(sole::uuid query_id, ::grpc::ServerContext* context) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];
  query_map.active_agent_contexts.insert(context);
}

// When a result stream is complete, we remove it from the list of stream contexts to
// cancel in the event that the query is cancelled.
void GRPCRouter::MarkResultStreamContextAsComplete(sole::uuid query_id,
                                                   ::grpc::ServerContext* context) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];
  query_map.active_agent_contexts.erase(context);
}

::grpc::Status GRPCRouter::TransferResultChunk(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::px::carnotpb::TransferResultChunkRequest>* reader,
    ::px::carnotpb::TransferResultChunkResponse* response) {
  PL_UNUSED(context);
  auto rb = std::make_unique<carnotpb::TransferResultChunkRequest>();

  // If this is a query result stream, these are used to track whether or not this particular
  // result stream has been closed so that downstream operators on the corresponding
  // source node can assess the health of the connection.
  // Ignored if this stream is used to send exec stats.
  // These assume that a given TransferResultChunk stream is only used to send the data for
  // a single result table.
  bool stream_has_query_results = false;
  int64_t source_node_id = 0;
  sole::uuid query_id;

  ::grpc::Status result_status = ::grpc::Status::OK;
  bool registered_server_context = false;

  while (reader->Read(rb.get())) {
    query_id = px::ParseUUID(rb->query_id()).ConsumeValueOrDie();
    if (!registered_server_context) {
      RegisterResultStreamContext(query_id, context);
      registered_server_context = true;
    }

    if (rb->has_execution_and_timing_info()) {
      std::vector<queryresultspb::AgentExecutionStats> stats;
      for (const auto& stat : rb->execution_and_timing_info().agent_execution_stats()) {
        stats.push_back(stat);
      }
      auto s = RecordStats(query_id, stats);
      if (!s.ok()) {
        result_status =
            ::grpc::Status(grpc::StatusCode::INTERNAL,
                           absl::Substitute("Failed to record stats w/ err: $0", s.msg()));
        break;
      }
    } else if (rb->has_query_result() && rb->query_result().has_row_batch()) {
      auto s = EnqueueRowBatch(query_id, std::move(rb));
      if (!s.ok()) {
        result_status = ::grpc::Status(grpc::StatusCode::INTERNAL, "failed to enqueue batch");
        break;
      }
    } else if (rb->has_query_result() && rb->query_result().initiate_result_stream()) {
      if (rb->query_result().destination_case() !=
          carnotpb::TransferResultChunkRequest_SinkResult::DestinationCase::kGrpcSourceId) {
        result_status = ::grpc::Status(grpc::StatusCode::INTERNAL,
                                       "expected result stream to have grpc source ID");
        break;
      }

      stream_has_query_results = true;
      source_node_id = rb->query_result().grpc_source_id();
      auto s = MarkResultStreamInitiated(query_id, source_node_id);
      if (!s.ok()) {
        result_status = ::grpc::Status(grpc::StatusCode::INTERNAL, s.msg());
        break;
      }

    } else {
      result_status =
          ::grpc::Status(grpc::StatusCode::INTERNAL,
                         "expected TransferResultChunkRequest to have either query_result "
                         "or execution_and_timing_info set, received neither");
      break;
    }

    rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
  }

  if (!result_status.ok()) {
    MarkResultStreamContextAsComplete(query_id, context);
    return result_status;
  }

  if (stream_has_query_results) {
    auto s = MarkResultStreamClosed(query_id, source_node_id);
    if (!s.ok()) {
      MarkResultStreamContextAsComplete(query_id, context);
      return ::grpc::Status(grpc::StatusCode::INTERNAL, s.msg());
    }
  }

  MarkResultStreamContextAsComplete(query_id, context);
  response->set_success(true);
  return ::grpc::Status::OK;
}

Status GRPCRouter::RecordStats(const sole::uuid& query_id,
                               const std::vector<queryresultspb::AgentExecutionStats>& stats) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto it = query_node_map_.find(query_id);
  if (it == query_node_map_.end()) {
    return error::Internal("No query ID $0 found in the GRPCRouter", query_id.str());
  }
  auto tracker = &it->second;
  for (const auto& agent : stats) {
    auto agent_id = px::ParseUUID(agent.agent_id()).ConsumeValueOrDie();
    // There are some cases where we get duplicate exec stats.
    if (tracker->seen_agents.contains(agent_id)) {
      continue;
    }
    tracker->agent_exec_stats.push_back(agent);
  }
  return Status::OK();
}

Status GRPCRouter::AddGRPCSourceNode(sole::uuid query_id, int64_t source_id,
                                     GRPCSourceNode* source_node,
                                     std::function<void()> restart_execution) {
  // We need to check and see if there is backlog data, if so flush it from the vector.
  SourceNodeTracker* snt = nullptr;
  {
    absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
    snt = &(query_node_map_[query_id].source_node_trackers[source_id]);
    query_node_map_[query_id].restart_execution_func_ = restart_execution;
  }
  absl::base_internal::SpinLockHolder snt_lock(&snt->node_lock);
  snt->source_node = source_node;
  if (snt->connection_initiated_by_sink) {
    source_node->set_upstream_initiated_connection();
  }
  if (snt->response_backlog.size() > 0) {
    for (auto& rb : snt->response_backlog) {
      PL_RETURN_IF_ERROR(snt->source_node->EnqueueRowBatch(std::move(rb)));
    }
    snt->response_backlog.clear();
  }
  if (snt->connection_closed_by_sink) {
    source_node->set_upstream_closed_connection();
  }

  return Status::OK();
}

StatusOr<std::vector<queryresultspb::AgentExecutionStats>> GRPCRouter::GetIncomingWorkerExecStats(
    const sole::uuid& query_id, const std::vector<uuidpb::UUID>& expected_agent_ids) {
  std::vector<queryresultspb::AgentExecutionStats> agent_exec_stats;
  {
    absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
    auto it = query_node_map_.find(query_id);
    if (it == query_node_map_.end()) {
      return error::Internal("No query ID $0 found in the GRPCRouter", query_id.str());
    }
    auto tracker = &it->second;
    agent_exec_stats = tracker->agent_exec_stats;
    if (agent_exec_stats.size() != expected_agent_ids.size()) {
      LOG(WARNING) << absl::Substitute("Agent ids are not the same size. Got $0 and expected $1",
                                       agent_exec_stats.size(), expected_agent_ids.size());
    }
  }
  return agent_exec_stats;
}

Status GRPCRouter::DeleteGRPCSourceNode(sole::uuid query_id, int64_t source_id) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  if (!query_node_map_.contains(query_id)) {
    return error::Internal("Query map does not contain query ID $0 when deleting GRPC source $1",
                           query_id.str(), source_id);
  }
  auto& query_map = query_node_map_[query_id];
  auto it = query_map.source_node_trackers.find(source_id);
  if (it == query_map.source_node_trackers.end()) {
    return error::Internal("Query map for query ID $0 does not contain GRPC source $1",
                           query_id.str(), source_id);
  }
  query_map.source_node_trackers.erase(it);
  return Status::OK();
}

void GRPCRouter::DeleteQuery(sole::uuid query_id) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  VLOG(1) << "Deleting query ID from GRPC Router: " << query_id.str();
  auto it = query_node_map_.find(query_id);
  if (it == query_node_map_.end()) {
    VLOG(1) << "No such query when deleting: " << query_id.str()
            << "(this is expected if no grpc sources are present)";
    return;
  }
  // For any active input streams for this query, mark their context as cancelled.
  for (auto ctx : query_node_map_[query_id].active_agent_contexts) {
    ctx->TryCancel();
  }
  query_node_map_.erase(it);
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
