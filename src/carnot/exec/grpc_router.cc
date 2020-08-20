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

namespace pl {
namespace carnot {
namespace exec {

Status GRPCRouter::EnqueueRowBatch(sole::uuid query_id,
                                   std::unique_ptr<carnotpb::TransferResultChunkRequest> req) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];

  if (!req->has_row_batch_result() ||
      req->row_batch_result().destination_case() !=
          carnotpb::TransferResultChunkRequest_ResultRowBatch::DestinationCase::kGrpcSourceId) {
    return error::Internal(
        "GRPCRouter::EnqueueRowBatch expected TransferResultChunkRequest to contain a row batch "
        "with a GPRC source ID.");
  }

  SourceNodeTracker& snt = query_map.source_node_trackers[req->row_batch_result().grpc_source_id()];
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

::grpc::Status GRPCRouter::TransferResultChunk(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::pl::carnotpb::TransferResultChunkRequest>* reader,
    ::pl::carnotpb::TransferResultChunkResponse* response) {
  PL_UNUSED(context);
  auto rb = std::make_unique<carnotpb::TransferResultChunkRequest>();

  while (reader->Read(rb.get())) {
    auto query_id = pl::ParseUUID(rb->query_id()).ConsumeValueOrDie();

    if (rb->has_execution_and_timing_info()) {
      std::vector<queryresultspb::AgentExecutionStats> stats;
      for (const auto& stat : rb->execution_and_timing_info().agent_execution_stats()) {
        stats.push_back(stat);
      }
      auto s = RecordStats(query_id, stats);
      if (!s.ok()) {
        return ::grpc::Status(grpc::StatusCode::INTERNAL,
                              absl::Substitute("Failed to record stats w/ err: $0", s.msg()));
      }
    } else if (rb->has_row_batch_result()) {
      auto s = EnqueueRowBatch(query_id, std::move(rb));
      if (!s.ok()) {
        return ::grpc::Status(grpc::StatusCode::INTERNAL, "failed to enqueue batch");
      }
    } else {
      return ::grpc::Status(grpc::StatusCode::INTERNAL,
                            "expected TransferResultChunkRequest to have either row_batch_result "
                            "or execution_and_timing_info set, received neither");
    }

    rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
  }

  response->set_success(true);
  return ::grpc::Status::OK;
}

::grpc::Status GRPCRouter::Done(::grpc::ServerContext*, const ::pl::carnotpb::DoneRequest* req,
                                ::pl::carnotpb::DoneResponse* resp) {
  auto query_id = pl::ParseUUID(req->query_id()).ConsumeValueOrDie();

  std::vector<queryresultspb::AgentExecutionStats> stats;
  for (const auto& stat : req->agent_execution_stats()) {
    stats.push_back(stat);
  }
  auto s = RecordStats(query_id, stats);
  if (!s.ok()) {
    return ::grpc::Status(grpc::StatusCode::INTERNAL,
                          absl::Substitute("Failed to record stats w/ err: $0", s.msg()));
  }
  resp->set_success(true);
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
    auto agent_id = pl::ParseUUID(agent.agent_id()).ConsumeValueOrDie();
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
  if (snt->response_backlog.size() > 0) {
    for (auto& rb : snt->response_backlog) {
      PL_RETURN_IF_ERROR(snt->source_node->EnqueueRowBatch(std::move(rb)));
    }
    snt->response_backlog.clear();
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
      return error::Internal("No query ID $0 found in the GRPCRouter");
    }
    auto tracker = &it->second;
    agent_exec_stats = tracker->agent_exec_stats;
    if (agent_exec_stats.size() != expected_agent_ids.size()) {
      LOG(ERROR) << absl::Substitute("Agent ids are not the same size. Got $0 and expected $1",
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
  query_node_map_.erase(it);
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
