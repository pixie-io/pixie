#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/carnotpb/carnot.grpc.pb.h"
#include "src/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

// This class provides a local GRPC server to receive results from benchmarks, other end-to-end
// tests, and the single node Carnot executable.
class LocalResultSinkServer final : public carnotpb::ResultSinkService::Service {
 public:
  const std::vector<carnotpb::TransferResultChunkRequest>& query_results() const {
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
      query_results_.push_back(*rb);
      rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
    }
    response->set_success(true);
    return ::grpc::Status::OK;
  }

  // Implements the Done of ResultSinkService.
  // TODO(nserrino): Remove when this API is deprecated.
  ::grpc::Status Done(::grpc::ServerContext*, const ::pl::carnotpb::DoneRequest*,
                      ::pl::carnotpb::DoneResponse* response) override {
    response->set_success(true);
    return ::grpc::Status::OK;
  }

 private:
  // List of the query results received.
  std::vector<carnotpb::TransferResultChunkRequest> query_results_;
};

class LocalGRPCResultSinkServer {
 public:
  explicit LocalGRPCResultSinkServer(int32_t port) : port_(port) {}

  void StartServerThread() {
    SetupServer();
    grpc_server_thread_ = std::make_unique<std::thread>(&LocalGRPCResultSinkServer::Wait, this);
  }

  ~LocalGRPCResultSinkServer() {
    if (grpc_server_) {
      grpc_server_->Shutdown();
    }
    if (grpc_server_thread_ && grpc_server_thread_->joinable()) {
      grpc_server_thread_->join();
    }
  }

  const std::vector<carnotpb::TransferResultChunkRequest>& query_results() const {
    return result_sink_server_.query_results();
  }

  std::unique_ptr<carnotpb::ResultSinkService::StubInterface> StubGenerator(
      const std::string&) const {
    grpc::ChannelArguments args;
    return pl::carnotpb::ResultSinkService::NewStub(grpc_server_->InProcessChannel(args));
  }

 private:
  void SetupServer() {
    std::string server_address(absl::Substitute("0.0.0.0:$0", port_));
    grpc::ServerBuilder builder;

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&result_sink_server_);
    grpc_server_ = builder.BuildAndStart();
    CHECK(grpc_server_ != nullptr);
  }

  void Wait() { grpc_server_->Wait(); }

  const int32_t port_;
  std::unique_ptr<std::thread> grpc_server_thread_;
  std::unique_ptr<grpc::Server> grpc_server_;
  LocalResultSinkServer result_sink_server_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
