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

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>
#include <grpcpp/grpcpp.h>

#include "src/carnot/exec/grpc_source_node.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"

namespace px {
namespace carnot {
namespace exec {

GRPCRouter::SourceNodeTracker* GRPCRouter::GetSourceNodeTracker(QueryTracker* query_tracker,
                                                                int64_t source_id) {
  absl::base_internal::SpinLockHolder query_lock(&query_tracker->query_lock);
  return &query_tracker->source_node_trackers[source_id];
}

Status GRPCRouter::EnqueueRowBatch(QueryTracker* query_tracker,
                                   std::unique_ptr<carnotpb::TransferResultChunkRequest> req) {
  if (!req->has_query_result() || !req->query_result().has_row_batch() ||
      req->query_result().destination_case() !=
          carnotpb::TransferResultChunkRequest_SinkResult::DestinationCase::kGrpcSourceId) {
    return error::Internal(
        "GRPCRouter::EnqueueRowBatch expected TransferResultChunkRequest to contain a row batch "
        "with a GPRC source ID.");
  }

  auto snt = GetSourceNodeTracker(query_tracker, req->query_result().grpc_source_id());
  {
    absl::base_internal::SpinLockHolder snt_lock(&snt->node_lock);
    // It's possible that we see row batches before we have gotten information about the query. To
    // solve this race, We store a backlog of all the pending batches.
    if (snt->source_node == nullptr) {
      snt->connection_initiated_by_sink = true;
      snt->response_backlog.emplace_back(std::move(req));
      return Status::OK();
    }
    snt->source_node->set_upstream_initiated_connection();
    PX_RETURN_IF_ERROR(snt->source_node->EnqueueRowBatch(std::move(req)));
  }
  query_tracker->RestartExecution();
  return Status::OK();
}

void GRPCRouter::MarkResultStreamClosed(QueryTracker* query_tracker, int64_t source_id) {
  auto snt = GetSourceNodeTracker(query_tracker, source_id);
  absl::base_internal::SpinLockHolder snt_lock(&snt->node_lock);
  // It's possible that we see row batches before we have gotten information about the query. To
  // solve this race, We store a backlog of all the pending batches.
  if (snt->source_node == nullptr) {
    DCHECK(!snt->connection_closed_by_sink);
    snt->connection_closed_by_sink = true;
    return;
  }
  DCHECK(!snt->source_node->upstream_closed_connection());
  snt->source_node->set_upstream_closed_connection();
  return;
}

// For all inbound result streams, we want to register the context of the stream,
// so that if the query gets cancelled, then we can make sure to also cancel the corresponding
// TransferResultChunk streams for that query.
void GRPCRouter::RegisterResultStreamContext(QueryTracker* query_tracker,
                                             ::grpc::ServerContext* context) {
  absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
  query_tracker->active_agent_contexts.insert(context);
}

// When a result stream is complete, we remove it from the list of stream contexts to
// cancel in the event that the query is cancelled.
void GRPCRouter::MarkResultStreamContextAsComplete(QueryTracker* query_tracker,
                                                   ::grpc::ServerContext* context) {
  absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
  query_tracker->active_agent_contexts.erase(context);
}

::grpc::Status GRPCRouter::HandleTransferResultChunkMessage(
    std::unique_ptr<carnotpb::TransferResultChunkRequest> req, ::grpc::ServerContext* context,
    TransferResultChunkState* state) {
  auto query_id = px::ParseUUID(req->query_id()).ConsumeValueOrDie();
  {
    absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
    if (!id_to_query_tracker_map_.contains(query_id)) {
      if (!req->has_initiate_conn()) {
        return ::grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            "Attempting to TransferResultChunk for uninitiated or completed query.");
      }
      id_to_query_tracker_map_[query_id] = std::make_shared<QueryTracker>();
    }
    state->query_tracker = id_to_query_tracker_map_[query_id];
  }

  if (!state->registered_server_context) {
    RegisterResultStreamContext(state->query_tracker.get(), context);
    state->registered_server_context = true;
  }

  if (req->has_initiate_conn()) {
    return ::grpc::Status::OK;
  }

  if (req->has_execution_and_timing_info()) {
    absl::base_internal::SpinLockHolder query_lock(&state->query_tracker->query_lock);
    for (const auto& agent : req->execution_and_timing_info().agent_execution_stats()) {
      auto agent_id = px::ParseUUID(agent.agent_id()).ConsumeValueOrDie();
      // There are some cases where we get duplicate exec stats.
      if (state->query_tracker->seen_agents.contains(agent_id)) {
        continue;
      }
      state->query_tracker->agent_exec_stats.push_back(agent);
    }
    return ::grpc::Status::OK;
  }
  if (req->has_query_result() && req->query_result().has_row_batch()) {
    state->stream_has_query_results = true;
    state->source_node_id = req->query_result().grpc_source_id();
    auto s = EnqueueRowBatch(state->query_tracker.get(), std::move(req));
    if (!s.ok()) {
      return ::grpc::Status(grpc::StatusCode::INTERNAL, "failed to enqueue batch");
    }
    return ::grpc::Status::OK;
  }
  if (req->has_execution_error()) {
    absl::base_internal::SpinLockHolder query_lock(&state->query_tracker->query_lock);
    state->query_tracker->upstream_exec_errors.push_back(req->execution_error());
    return ::grpc::Status::OK;
  }
  return ::grpc::Status(grpc::StatusCode::INTERNAL, "unexpected TransferResultChunkRequest type");
}
std::vector<statuspb::Status> GRPCRouter::GetIncomingWorkerErrors(const sole::uuid& query_id) {
  std::shared_ptr<QueryTracker> query_tracker;
  {
    absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
    if (!id_to_query_tracker_map_.contains(query_id)) {
      return {};
    }
    query_tracker = id_to_query_tracker_map_[query_id];
  }
  absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
  return query_tracker->upstream_exec_errors;
}

::grpc::Status GRPCRouter::TransferResultChunk(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::px::carnotpb::TransferResultChunkRequest>* reader,
    ::px::carnotpb::TransferResultChunkResponse* response) {
  ::grpc::Status result_status = ::grpc::Status::OK;
  auto req = std::make_unique<carnotpb::TransferResultChunkRequest>();
  TransferResultChunkState state;
  while (reader->Read(req.get())) {
    result_status = HandleTransferResultChunkMessage(std::move(req), context, &state);
    if (!result_status.ok()) {
      break;
    }
    req = std::make_unique<carnotpb::TransferResultChunkRequest>();
  }

  if (state.query_tracker != nullptr) {
    MarkResultStreamContextAsComplete(state.query_tracker.get(), context);
  }
  if (!result_status.ok()) {
    return result_status;
  }

  if (state.query_tracker == nullptr) {
    // In this case, the client immediately finished writing without sending a query id so no
    // query_tracker pointer was set.
    return ::grpc::Status::OK;
  }

  if (state.stream_has_query_results) {
    MarkResultStreamClosed(state.query_tracker.get(), state.source_node_id);
  }

  response->set_success(true);
  return ::grpc::Status::OK;
}

Status GRPCRouter::AddGRPCSourceNode(sole::uuid query_id, int64_t source_id,
                                     GRPCSourceNode* source_node,
                                     std::function<void()> restart_execution) {
  // We need to check and see if there is backlog data, if so flush it from the vector.
  std::shared_ptr<QueryTracker> query_tracker;
  {
    absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
    if (!id_to_query_tracker_map_.contains(query_id)) {
      id_to_query_tracker_map_[query_id] = std::make_shared<QueryTracker>();
    }
    query_tracker = id_to_query_tracker_map_[query_id];
  }

  {
    absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
    query_tracker->restart_execution_func_ = std::move(restart_execution);
  }
  auto snt = GetSourceNodeTracker(query_tracker.get(), source_id);

  absl::base_internal::SpinLockHolder snt_lock(&snt->node_lock);
  snt->source_node = source_node;
  if (snt->connection_initiated_by_sink) {
    source_node->set_upstream_initiated_connection();
  }
  if (snt->response_backlog.size() > 0) {
    for (auto& rb : snt->response_backlog) {
      PX_RETURN_IF_ERROR(snt->source_node->EnqueueRowBatch(std::move(rb)));
    }
    snt->response_backlog.clear();
  }
  if (snt->connection_closed_by_sink) {
    source_node->set_upstream_closed_connection();
  }

  return Status::OK();
}

StatusOr<std::vector<queryresultspb::AgentExecutionStats>> GRPCRouter::GetIncomingWorkerExecStats(
    const sole::uuid& query_id) {
  std::shared_ptr<QueryTracker> query_tracker;
  {
    absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
    auto it = id_to_query_tracker_map_.find(query_id);
    if (it == id_to_query_tracker_map_.end()) {
      return error::Internal("No query ID $0 found in the GRPCRouter", query_id.str());
    }
    query_tracker = it->second;
  }

  {
    absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
    return query_tracker->agent_exec_stats;
  }
}

Status GRPCRouter::DeleteGRPCSourceNode(sole::uuid query_id, int64_t source_id) {
  std::shared_ptr<QueryTracker> query_tracker;
  {
    absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
    if (!id_to_query_tracker_map_.contains(query_id)) {
      return error::Internal("Query map does not contain query ID $0 when deleting GRPC source $1",
                             query_id.str(), source_id);
    }
    query_tracker = id_to_query_tracker_map_[query_id];
  }

  absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
  auto it = query_tracker->source_node_trackers.find(source_id);
  if (it == query_tracker->source_node_trackers.end()) {
    return error::Internal("Query map for query ID $0 does not contain GRPC source $1",
                           query_id.str(), source_id);
  }
  query_tracker->source_node_trackers.erase(it);
  return Status::OK();
}

void GRPCRouter::DeleteQuery(sole::uuid query_id) {
  VLOG(1) << "Deleting query ID from GRPC Router: " << query_id.str();
  std::shared_ptr<QueryTracker> query_tracker;
  {
    absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
    auto it = id_to_query_tracker_map_.find(query_id);
    if (it == id_to_query_tracker_map_.end()) {
      VLOG(1) << "No such query when deleting: " << query_id.str()
              << "(this is expected if no grpc sources are present)";
      return;
    }
    query_tracker = it->second;
    id_to_query_tracker_map_.erase(it);
  }
  absl::base_internal::SpinLockHolder lock(&query_tracker->query_lock);
  query_tracker->ResetRestartExecutionFunc();
  // For any active input streams for this query, mark their context as cancelled.
  for (auto ctx : query_tracker->active_agent_contexts) {
    ctx->TryCancel();
  }
}

size_t GRPCRouter::NumQueriesTracking() const {
  absl::base_internal::SpinLockHolder lock(&id_to_query_tracker_map_lock_);
  return id_to_query_tracker_map_.size();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
