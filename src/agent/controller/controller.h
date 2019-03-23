#pragma once

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <sole.hpp>

#include "src/carnot/carnot.h"
#include "src/stirling/stirling.h"

#include "src/common/base/base.h"
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
  /**
   * Create a new controller. Expects carnot and stirling to be initialized.
   */
  static StatusOr<std::unique_ptr<Controller>> Create(std::shared_ptr<grpc::Channel> chan,
                                                      std::unique_ptr<carnot::Carnot> carnot,
                                                      std::unique_ptr<stirling::Stirling> stirling);

  ~Controller() = default;

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

  // TODO(zasgar): Remove me. Throwaway code for demo. We can't call this from Init because
  // it will break tests and there is no way to stub out stirling.
  Status InitThrowaway();

 protected:
  Controller(std::shared_ptr<grpc::Channel> chan, std::unique_ptr<carnot::Carnot> carnot,
             std::unique_ptr<stirling::Stirling> stirling);
  /**
   * Initialize the executor.
   */
  Status Init();

 private:
  // TODO(zasgar): Remove me. Throwaway code for demo.
  Status AddDummyTable(const std::string& name, std::shared_ptr<carnot::schema::Table> table);

  void RunHeartBeat(VizierReaderWriter* stream) const;
  Status ExecuteQuery(const vizier::QueryRequest& req, vizier::AgentQueryResponse* resp);
  Status SendRegisterRequest(VizierReaderWriter* stream);

  std::shared_ptr<grpc::Channel> chan_;
  std::unique_ptr<carnot::Carnot> carnot_;
  std::unique_ptr<stirling::Stirling> stirling_;

  sole::uuid agent_id_;
  std::string hostname_;
  std::unique_ptr<vizier::VizierService::Stub> vizier_stub_;
  bool keepAlive_ = true;
};

}  // namespace agent
}  // namespace pl
