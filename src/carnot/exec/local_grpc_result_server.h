#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/carnot/carnotpb/carnot.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {

using table_store::schema::RowBatch;
using QueryExecStats = carnotpb::TransferResultChunkRequest_QueryExecutionAndTimingInfo;

namespace exec {

// This class provides a local GRPC server to receive results from benchmarks, other end-to-end
// tests, and the single node Carnot executable.
class LocalResultSinkServer final : public carnotpb::ResultSinkService::Service {
 public:
  std::vector<carnotpb::TransferResultChunkRequest> query_results() {
    const std::lock_guard<std::mutex> lock(result_mutex_);
    return query_results_;
  }

  // Implements the TransferResultChunkAPI of ResultSinkService.
  ::grpc::Status TransferResultChunk(
      ::grpc::ServerContext*,
      ::grpc::ServerReader<::pl::carnotpb::TransferResultChunkRequest>* reader,
      ::pl::carnotpb::TransferResultChunkResponse* response) override {
    auto rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
    // Write the results to the result vector.

    while (reader->Read(rb.get())) {
      const std::lock_guard<std::mutex> lock(result_mutex_);
      query_results_.push_back(*rb);
      rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
    }
    response->set_success(true);
    return ::grpc::Status::OK;
  }

 private:
  // List of the query results received.
  std::vector<carnotpb::TransferResultChunkRequest> query_results_;
  // Mutex to handle concurrent calls to TransferResultChunk.
  std::mutex result_mutex_;
};

class LocalGRPCResultSinkServer {
 public:
  LocalGRPCResultSinkServer() {
    grpc::ServerBuilder builder;

    builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials());
    builder.RegisterService(&result_sink_server_);
    grpc_server_ = builder.BuildAndStart();
    CHECK(grpc_server_ != nullptr);
  }

  ~LocalGRPCResultSinkServer() {
    if (grpc_server_) {
      grpc_server_->Shutdown();
    }
  }

  std::vector<carnotpb::TransferResultChunkRequest> raw_query_results() {
    return result_sink_server_.query_results();
  }

  StatusOr<QueryExecStats> exec_stats() {
    bool got_exec_stats = false;
    QueryExecStats output;
    for (const auto& req : result_sink_server_.query_results()) {
      if (req.has_execution_and_timing_info()) {
        if (got_exec_stats) {
          return error::Internal(
              "Exec stats result chunk was unexpectedly sent twice for one query");
        }
        output = req.execution_and_timing_info();
        got_exec_stats = true;
      }
    }
    return output;
  }

  absl::flat_hash_set<std::string> output_tables() {
    absl::flat_hash_set<std::string> output;
    for (const auto& req : result_sink_server_.query_results()) {
      if (req.has_query_result()) {
        output.insert(req.query_result().table_name());
      }
    }
    return output;
  }

  std::vector<RowBatch> query_results(std::string_view table_name) {
    std::vector<RowBatch> output;
    for (const auto& req : result_sink_server_.query_results()) {
      if (req.has_query_result() && req.query_result().has_row_batch() &&
          req.query_result().table_name() == table_name) {
        auto rb = RowBatch::FromProto(req.query_result().row_batch()).ConsumeValueOrDie();
        output.push_back(*rb);
      }
    }
    return output;
  }

  std::unique_ptr<carnotpb::ResultSinkService::StubInterface> StubGenerator(
      const std::string&) const {
    grpc::ChannelArguments args;
    return pl::carnotpb::ResultSinkService::NewStub(grpc_server_->InProcessChannel(args));
  }

 private:
  std::unique_ptr<grpc::Server> grpc_server_;
  LocalResultSinkServer result_sink_server_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
