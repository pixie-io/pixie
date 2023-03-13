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

#include <memory>

#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

class HeartbeatMessageHandler : public Manager::MessageHandler {
 public:
  HeartbeatMessageHandler() = delete;
  HeartbeatMessageHandler(px::event::Dispatcher* dispatcher,
                          px::md::AgentMetadataStateManager* mds_manager,
                          RelationInfoManager* relation_info_manager, Info* agent_info,
                          Manager::VizierNATSConnector* nats_conn);

  ~HeartbeatMessageHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;
  void DisableHeartbeats();
  void EnableHeartbeats();

 private:
  void ConsumeAgentPIDUpdates(messages::AgentUpdateInfo* update_info);
  void ProcessPIDStartedEvent(const px::md::PIDStartedEvent& ev,
                              messages::AgentUpdateInfo* update_info);

  void ProcessPIDTerminatedEvent(const px::md::PIDTerminatedEvent& ev,
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

  std::unique_ptr<px::vizier::messages::VizierMessage> last_sent_hb_;
  int64_t last_metadata_epoch_id_ = 0;
  bool sent_schema_ = false;

  HeartbeatInfo heartbeat_info_;
  const px::event::TimeSource& time_source_;
  px::md::AgentMetadataStateManager* mds_manager_;
  RelationInfoManager* relation_info_manager_;
  std::chrono::duration<double> heartbeat_latency_moving_average_{0};

  px::event::TimerUPtr heartbeat_send_timer_;
  px::event::TimerUPtr heartbeat_watchdog_timer_;

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
  HeartbeatNackMessageHandler(px::event::Dispatcher* dispatcher, Info* agent_info,
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
}  // namespace px
