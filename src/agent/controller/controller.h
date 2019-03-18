#pragma once

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <sole.hpp>

#include "src/agent/controller/executor.h"

#include "src/common/common.h"
PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/proto/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace agent {

/**
 * The interval to retry connections o vizier.
 */
constexpr int kAgentConnectRetryIntervalSeconds = 1;
/**
 * The interval on which heartbeats are sent to vizier.
 */
constexpr int kAgentHeartBeatIntervalSeconds = 5;

/**
 * Controller is responsible for managing and orchestrating the
 * components that make up the agent.
 *
 * The controller contains the connection to Vizier.
 */
class Controller : public NotCopyable {
  using VizierReaderWriter =
      grpc::ClientReaderWriter<vizier::AgentToVizierMessage, vizier::VizierToAgentMessage>;
  using VizierReaderWriterSPtr = std::shared_ptr<VizierReaderWriter>;

 public:
  Controller(std::shared_ptr<grpc::Channel> chan, Executor* executor);
  ~Controller() = default;

  Status Init();

  /**
   * Main run loop for the agent. It blocks until the stop signal is sent.
   * @return Status of the run loop.
   */
  Status Run();

  /**
   * Stop all executing threads.
   * @return Status of stopping.
   */
  Status Stop();

 private:
  void RunHeartBeat(VizierReaderWriter* stream) const;
  Status ExecuteQuery(const vizier::QueryRequest& req, vizier::AgentQueryResponse* resp);
  Status SendRegisterRequest(VizierReaderWriter* stream);

  Executor* executor_;

  sole::uuid agent_id_;
  std::string hostname_;
  std::shared_ptr<grpc::Channel> chan_;
  std::unique_ptr<vizier::VizierService::Stub> vizier_stub_;
  bool keepAlive_ = true;
};

}  // namespace agent
}  // namespace pl
