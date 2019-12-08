#pragma once

#include <nats/nats.h>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <sole.hpp>

#include "src/carnot/carnot.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/stirling.h"
#include "src/vizier/services/agent/controller/relation_info_manager.h"

#include "src/common/base/base.h"
#include "src/common/nats/nats.h"

PL_SUPPRESS_WARNINGS_START()
// TODO(michelle): Fix this so that we don't need to the NOLINT.
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()
#include "src/vizier/messages/messagespb/messages.pb.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/services/query_broker/querybrokerpb/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace vizier {
namespace agent {

/**
 * Controller is responsible for managing and orchestrating the
 * components that make up the agent.
 *
 * The controller contains the connection to Vizier.
 */
class Controller : public NotCopyable {
 public:
  using VizierNATSTLSConfig = pl::nats::NATSTLSConfig;
  using VizierNATSConnector = pl::nats::NATSConnector<pl::vizier::messages::VizierMessage>;
  using QBStub =
      pl::vizier::services::query_broker::querybrokerpb::QueryBrokerService::StubInterface;
  /**
   * Create a new controller. Expects carnot and stirling to be initialized.
   */
  static StatusOr<std::unique_ptr<Controller>> Create(
      sole::uuid agent_id, std::unique_ptr<QBStub> queryBrokerChan,
      std::unique_ptr<carnot::Carnot> carnot, std::unique_ptr<stirling::Stirling> stirling,
      std::shared_ptr<table_store::TableStore> table_store,
      std::unique_ptr<VizierNATSConnector> nats_connector);

  ~Controller();

  /**
   * Main run loop for the agent. It blocks until the stop signal is sent.
   * @return Status of the run loop.
   */
  Status Run();

  /**
   * Stop main execution thread.
   * This is a blocking call that waits only for the main thread to stop.
   * It does not check on the state of any worker threads.
   *
   * @param timeout Duration after which the call will return, even if thread is not stopped.
   * @return Status of stopping.
   */
  Status Stop(std::chrono::milliseconds timeout);

  // TODO(zasgar): Remove me. Throwaway code for demo. We can't call this from Init because
  // it will break tests and there is no way to stub out stirling.
  Status InitSchemas();

 protected:
  Controller(sole::uuid agent_id, std::unique_ptr<QBStub> queryBrokerChan,
             std::unique_ptr<carnot::Carnot> carnot, std::unique_ptr<stirling::Stirling> stirling,
             std::shared_ptr<table_store::TableStore> table_store,
             std::unique_ptr<VizierNATSConnector> nats_connector);
  /**
   * Initialize the executor.
   */
  Status Init();

 private:
  Status ExecuteQuery(const messages::ExecuteQueryRequest& req,
                      pl::vizier::services::query_broker::querybrokerpb::AgentQueryResponse* resp);

  void RunHeartbeat();
  Status RegisterAgent();

  Status HandleHeartbeatMessage(std::unique_ptr<messages::VizierMessage> msg);
  Status HandleExecuteQueryMessage(std::unique_ptr<messages::VizierMessage> msg);

  Status HandleMDSUpdates(const messages::MetadataUpdateInfo& update_info);
  void ConsumeAgentPIDUpdates(messages::AgentUpdateInfo* update_info);

  static void ProcessPIDStartedEvent(const pl::md::PIDStartedEvent& ev,
                                     messages::AgentUpdateInfo* update_info);
  static void ProcessPIDTerminatedEvent(const pl::md::PIDTerminatedEvent& ev,
                                        messages::AgentUpdateInfo* update_info);
  void StartHelperThreads();
  Status WaitForHelperThreads();

  // We direct heartbeat messages to this queue.
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<messages::VizierMessage>>
      incoming_heartbeat_queue_;

  std::unique_ptr<QBStub> qb_stub_;
  std::unique_ptr<carnot::Carnot> carnot_;
  std::unique_ptr<stirling::Stirling> stirling_;
  std::shared_ptr<table_store::TableStore> table_store_;

  sole::uuid agent_id_;
  // Agent short Id.
  uint32_t asid_ = 0;
  std::string hostname_;

  std::unique_ptr<VizierNATSConnector> nats_connector_;
  std::unique_ptr<std::thread> heartbeat_thread_;

  std::atomic<bool> keep_alive_ = true;
  std::atomic<bool> running_ = false;

  std::unique_ptr<pl::md::AgentMetadataStateManager> mds_manager_;
  std::unique_ptr<std::thread> mds_thread_;

  std::unique_ptr<RelationInfoManager> relation_info_manager_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
