#pragma once

#include <memory>

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

class HeartbeatMessageHandler : public Manager::MessageHandler {
 public:
  HeartbeatMessageHandler() = delete;
  HeartbeatMessageHandler(pl::event::Dispatcher* dispatcher,
                          pl::md::AgentMetadataStateManager* mds_manager,
                          RelationInfoManager* relation_info_manager, Info* agent_info,
                          Manager::VizierNATSConnector* nats_conn);

  ~HeartbeatMessageHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;
  void DisableHeartbeats();
  void EnableHeartbeats();

 private:
  void ConsumeAgentPIDUpdates(messages::AgentUpdateInfo* update_info);
  void ProcessPIDStartedEvent(const pl::md::PIDStartedEvent& ev,
                              messages::AgentUpdateInfo* update_info);

  void ProcessPIDTerminatedEvent(const pl::md::PIDTerminatedEvent& ev,
                                 messages::AgentUpdateInfo* update_info);

  void DoHeartbeats();

  void SendHeartbeat();
  Status SendHeartbeatInternal();
  void HeartbeatWatchdog();

  struct HeartbeatInfo {
    HeartbeatInfo() = default;
    int64_t last_sent_seq_num = -1;
    int64_t last_ackd_seq_num = -1;
    std::chrono::steady_clock::time_point last_heartbeat_send_time_;
  };

  std::unique_ptr<pl::vizier::messages::VizierMessage> last_sent_hb_;
  uint64_t last_metadata_epoch_id_ = 0;

  HeartbeatInfo heartbeat_info_;
  const pl::event::TimeSource& time_source_;
  pl::md::AgentMetadataStateManager* mds_manager_;
  RelationInfoManager* relation_info_manager_;
  std::chrono::duration<double> heartbeat_latency_moving_average_{0};

  pl::event::TimerUPtr heartbeat_send_timer_;
  pl::event::TimerUPtr heartbeat_watchdog_timer_;

  static constexpr double kHbLatencyDecay = 0.25;

  static constexpr std::chrono::seconds kAgentHeartbeatInterval{5};
  static constexpr int kHeartbeatRetryCount = 5;
  // The amount of time to wait for a heartbeat ack.
  static constexpr std::chrono::milliseconds kHeartbeatWaitMillis{5000};
};

using HeartbeatReregisterHook = std::function<Status()>;

// Handles heartbeat nacks. Needs to be separate from heartbeat handler
// because heartbeat nacks will cause the heartbeat handler to be deleted
// and recreated.
class HeartbeatNackMessageHandler : public Manager::MessageHandler {
 public:
  HeartbeatNackMessageHandler() = delete;
  HeartbeatNackMessageHandler(pl::event::Dispatcher* dispatcher, Info* agent_info,
                              Manager::VizierNATSConnector* nats_conn,
                              HeartbeatReregisterHook reregister_hook)
      : MessageHandler(dispatcher, agent_info, nats_conn), reregister_hook_(reregister_hook) {}

  ~HeartbeatNackMessageHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  HeartbeatReregisterHook reregister_hook_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
