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

#include "src/carnotpb/carnot.grpc.pb.h"
#include "src/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"

namespace pl {
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
      ::grpc::ServerReader<::pl::carnotpb::TransferResultChunkRequest>* reader,
      ::pl::carnotpb::TransferResultChunkResponse* response) override;

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
   * @brief Get the Exec stats from the agents that are clients to this GRPC and the query_id.
   *
   * @param query_id
   * @return StatusOr<std::vector<queryresultspb::AgentExecutionStats>>
   */
  StatusOr<std::vector<queryresultspb::AgentExecutionStats>> GetIncomingWorkerExecStats(
      const sole::uuid& query_id, const std::vector<uuidpb::UUID>& expected_agent_ids);

  /**
   * @brief Records the execution statistics from the passed in request.
   *
   * @param query_id the query_id which to record the statistics.
   * @param stats the protobuf of the exec stats received by the agent.
   * @return Status
   */
  Status RecordStats(const sole::uuid& query_id,
                     const std::vector<queryresultspb::AgentExecutionStats>& stats);

 private:
  Status EnqueueRowBatch(sole::uuid query_id,
                         std::unique_ptr<carnotpb::TransferResultChunkRequest> req);

  Status MarkResultStreamInitiated(sole::uuid query_id, int64_t source_id);
  Status MarkResultStreamClosed(sole::uuid query_id, int64_t source_id);
  void RegisterResultStreamContext(sole::uuid query_id, ::grpc::ServerContext* context);
  void MarkResultStreamContextAsComplete(sole::uuid query_id, ::grpc::ServerContext* context);

  /**
   * SourceNodeTracker is responsible for tracking a single source node and the backlog of messages
   * for the source node.
   */
  struct SourceNodeTracker {
    SourceNodeTracker() = default;
    GRPCSourceNode* source_node GUARDED_BY(node_lock) = nullptr;
    bool connection_initiated_by_sink GUARDED_BY(node_lock) = false;
    bool connection_closed_by_sink GUARDED_BY(node_lock) = false;
    std::vector<std::unique_ptr<::pl::carnotpb::TransferResultChunkRequest>> response_backlog
        GUARDED_BY(node_lock);
    absl::base_internal::SpinLock node_lock;
  };

  /**
   * Query tracker tracks execution of a single query.
   */
  struct QueryTracker {
    QueryTracker() : create_time(std::chrono::steady_clock::now()) {}
    absl::node_hash_map<int64_t, SourceNodeTracker> source_node_trackers;
    std::chrono::steady_clock::time_point create_time;
    std::function<void()> restart_execution_func_;
    // The set of agents we've seen for the query.
    absl::flat_hash_set<sole::uuid> seen_agents;
    absl::flat_hash_set<::grpc::ServerContext*> active_agent_contexts;
    // The execution stats for agents that are clients to this service.
    std::vector<queryresultspb::AgentExecutionStats> agent_exec_stats;
  };

  absl::node_hash_map<sole::uuid, QueryTracker> query_node_map_ GUARDED_BY(query_node_map_lock_);
  absl::base_internal::SpinLock query_node_map_lock_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
