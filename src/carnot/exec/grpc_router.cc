#include <grpcpp/grpcpp.h>

#include <utility>

#include "absl/container/flat_hash_map.h"
#include "src/carnot/exec/grpc_router.h"
#include "src/carnot/exec/grpc_source_node.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"

PL_SUPPRESS_WARNINGS_START()
#include "include/grpcpp/server_context.h"
#include "include/grpcpp/support/status.h"
#include "include/grpcpp/support/sync_stream.h"
#include "src/carnotpb/carnot.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace carnot {
namespace exec {

Status GRPCRouter::EnqueueRowBatch(sole::uuid query_id,
                                   std::unique_ptr<carnotpb::RowBatchRequest> req) {
  absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
  auto& query_map = query_node_map_[query_id];
  SourceNodeTracker& snt = query_map.source_node_trackers[req->destination_id()];
  absl::base_internal::SpinLockHolder snt_lock(&snt.node_lock);
  // It's possible that we see row batches before we have gotten information about the query. To
  // solve this race, We store a backlog of all the pending batches.
  if (snt.source_node == nullptr) {
    snt.response_backlog.emplace_back(std::move(req));
    return Status::OK();
  }
  return snt.source_node->EnqueueRowBatch(std::move(req));
}

::grpc::Status GRPCRouter::TransferRowBatch(
    ::grpc::ServerContext* context, ::grpc::ServerReader<::pl::carnotpb::RowBatchRequest>* reader,
    ::pl::carnotpb::RowBatchResponse* response) {
  PL_UNUSED(context);
  auto rb = std::make_unique<carnotpb::RowBatchRequest>();
  while (reader->Read(rb.get())) {
    auto query_id = pl::ParseUUID(rb->query_id()).ConsumeValueOrDie();

    auto s = EnqueueRowBatch(query_id, std::move(rb));
    if (!s.ok()) {
      return ::grpc::Status(grpc::StatusCode::INTERNAL, "failed to enqueue batch");
    }

    rb = std::make_unique<carnotpb::RowBatchRequest>();
  }

  response->set_success(true);
  return ::grpc::Status::OK;
}

Status GRPCRouter::AddGRPCSourceNode(sole::uuid query_id, int64_t source_id,
                                     GRPCSourceNode* source_node) {
  // We need to check and see if there is backlog data, if so flush it from the vector.
  SourceNodeTracker* snt = nullptr;
  {
    absl::base_internal::SpinLockHolder lock(&query_node_map_lock_);
    snt = &(query_node_map_[query_id].source_node_trackers[source_id]);
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
