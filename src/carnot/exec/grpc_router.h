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

#include <stdint.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/node_hash_map.h>
#include <absl/hash/hash.h>
#include <grpcpp/grpcpp.h>
#include <sole.hpp>

#include "src/carnot/carnotpb/carnot.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/common/base/statuspb/status.pb.h"
#include "src/common/uuid/uuid.h"

namespace px {
namespace carnot {
namespace exec {

// Forward declaration needed to break circular dependency.
class GRPCSourceNode;

/**
 * GRPCRouter tracks incoming Kelvin connections and routes them to the appropriate Carnot source
 * node.
 */
class GRPCRouter final : public carnotpb::ResultSinkService::Service {
 public:
  /**
   * TransferResultChunk implements the RPC method.
   */
  ::grpc::Status TransferResultChunk(
      ::grpc::ServerContext* context,
      ::grpc::ServerReader<::px::carnotpb::TransferResultChunkRequest>* reader,
      ::px::carnotpb::TransferResultChunkResponse* response) override;

  /**
   * Adds the specified source node to the router. Includes a function that should be called to
   * retrigger execution of the graph if currently yielded.
   */
  Status AddGRPCSourceNode(sole::uuid query_id, int64_t source_id, GRPCSourceNode* source_node,
                           std::function<void()> restart_execution);

  /**
   * Delete all the metadata and backlog data for a query. Deleting a non-existing query is ignored.
   * @param query_id
   */
  void DeleteQuery(sole::uuid query_id);

  /**
   * Delete a source node for a query once that source node is no longer valid.
   * Used to guard against the case where data comes in for a source after it has timed out.
   * @param query_id
   */
  Status DeleteGRPCSourceNode(sole::uuid query_id, int64_t source_id);

  /**
   * @brief Get any errors that may have occured in the incoming worker nodes.
   *
   * @param query_id
   * @return StatusOr<std::vector<statuspb::Status>>
   */
  std::vector<statuspb::Status> GetIncomingWorkerErrors(const sole::uuid& query_id);

  /**
   * @brief Get the Exec stats from the agents that are clients to this GRPC and the query_id.
   *
   * @param query_id
   * @return StatusOr<std::vector<queryresultspb::AgentExecutionStats>>
   */
  StatusOr<std::vector<queryresultspb::AgentExecutionStats>> GetIncomingWorkerExecStats(
      const sole::uuid& query_id);

  /**
   * @brief Number of queries currently being tracked.
   * @return size_t number of queries being tracked.
   */
  size_t NumQueriesTracking() const;

 private:
  /**
   * SourceNodeTracker is responsible for tracking a single source node and the backlog of messages
   * for the source node.
   */
  struct SourceNodeTracker {
    SourceNodeTracker() = default;
    GRPCSourceNode* source_node GUARDED_BY(node_lock) = nullptr;
    // connection_initiated_by_sink and connection_closed_by_sink are true when the
    // grpc sink (aka the client) initiates the query result stream or closes a query result stream,
    // respectively.
    bool connection_initiated_by_sink GUARDED_BY(node_lock) = false;
    bool connection_closed_by_sink GUARDED_BY(node_lock) = false;
    std::vector<std::unique_ptr<::px::carnotpb::TransferResultChunkRequest>> response_backlog
        GUARDED_BY(node_lock);
    absl::base_internal::SpinLock node_lock;
  };

  /**
   * Query tracker tracks execution of a single query.
   */
  struct QueryTracker {
    QueryTracker() : create_time(std::chrono::steady_clock::now()) {}
    absl::node_hash_map<int64_t, SourceNodeTracker> source_node_trackers GUARDED_BY(query_lock);
    const std::chrono::steady_clock::time_point create_time GUARDED_BY(query_lock);
    std::function<void()> restart_execution_func_ GUARDED_BY(query_lock);
    // The set of agents we've seen for the query.
    absl::flat_hash_set<sole::uuid> seen_agents GUARDED_BY(query_lock);
    absl::flat_hash_set<::grpc::ServerContext*> active_agent_contexts GUARDED_BY(query_lock);
    // The execution stats for agents that are clients to this service.
    std::vector<queryresultspb::AgentExecutionStats> agent_exec_stats GUARDED_BY(query_lock);

    // Errors that occur during execution from parent_agents.
    std::vector<statuspb::Status> upstream_exec_errors GUARDED_BY(query_lock);
    absl::base_internal::SpinLock query_lock;

    void ResetRestartExecutionFunc() ABSL_EXCLUSIVE_LOCKS_REQUIRED(query_lock) {
      restart_execution_func_ = std::function<void()>();
    }

    void RestartExecution() {
      std::function<void()> restart_func;
      {
        absl::base_internal::SpinLockHolder lock(&query_lock);
        restart_func = restart_execution_func_;
      }
      // Check that restart_func is not an empty function.
      if (restart_func) {
        restart_func();
      }
    }
  };

  Status EnqueueRowBatch(QueryTracker* query_tracker,
                         std::unique_ptr<carnotpb::TransferResultChunkRequest> req);

  struct TransferResultChunkState {
    int64_t source_node_id = 0;
    bool registered_server_context = false;
    // stream_has_query_results informs downstream source nodes about the health of the stream.
    // When true, the particular TransferResultChunk call has initiated the query stream.
    bool stream_has_query_results = false;
    std::shared_ptr<QueryTracker> query_tracker = nullptr;
  };
  ::grpc::Status HandleTransferResultChunkMessage(
      std::unique_ptr<::px::carnotpb::TransferResultChunkRequest> req,
      ::grpc::ServerContext* context, TransferResultChunkState* state);

  void MarkResultStreamClosed(QueryTracker* query_tracker, int64_t source_id);
  void RegisterResultStreamContext(QueryTracker* query_tracker, ::grpc::ServerContext* context);
  void MarkResultStreamContextAsComplete(QueryTracker* query_tracker,
                                         ::grpc::ServerContext* context);
  SourceNodeTracker* GetSourceNodeTracker(QueryTracker* query_tracker, int64_t source_id);

  absl::node_hash_map<sole::uuid, std::shared_ptr<QueryTracker>> id_to_query_tracker_map_
      GUARDED_BY(id_to_query_tracker_map_lock_);
  mutable absl::base_internal::SpinLock id_to_query_tracker_map_lock_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
