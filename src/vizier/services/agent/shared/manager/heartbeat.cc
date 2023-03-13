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

#include "src/vizier/services/agent/shared/manager/heartbeat.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

using ::px::event::Dispatcher;
using ::px::shared::k8s::metadatapb::ResourceUpdate;

const int64_t kCmdlineTruncationLimit = 4096;
static constexpr char kTruncatedMsg[] = "... [TRUNCATED]";

HeartbeatMessageHandler::HeartbeatMessageHandler(Dispatcher* d,
                                                 px::md::AgentMetadataStateManager* mds_manager,
                                                 RelationInfoManager* relation_info_manager,
                                                 Info* agent_info,
                                                 Manager::VizierNATSConnector* nats_conn)
    : MessageHandler(d, agent_info, nats_conn),
      time_source_(dispatcher()->GetTimeSource()),
      mds_manager_(mds_manager),
      relation_info_manager_(relation_info_manager),
      heartbeat_send_timer_(
          dispatcher()->CreateTimer(std::bind(&HeartbeatMessageHandler::SendHeartbeat, this))),
      heartbeat_watchdog_timer_(
          dispatcher()->CreateTimer(std::bind(&HeartbeatMessageHandler::HeartbeatWatchdog, this))) {
  EnableHeartbeats();
}

void HeartbeatMessageHandler::DisableHeartbeats() {
  last_metadata_epoch_id_ = 0;
  sent_schema_ = false;
  heartbeat_send_timer_->DisableTimer();
  heartbeat_watchdog_timer_->DisableTimer();
}

void HeartbeatMessageHandler::EnableHeartbeats() {
  heartbeat_send_timer_->EnableTimer(std::chrono::milliseconds(0));
  heartbeat_watchdog_timer_->EnableTimer(kHeartbeatWaitMillis);
}

void HeartbeatMessageHandler::SendHeartbeat() {
  Status s = SendHeartbeatInternal();
  if (!s.ok()) {
    LOG(ERROR) << "Failed to send heartbeat, will retry on next tick. hb_info: "
               << last_sent_hb_->DebugString() << " error_message: " << s.msg();
  }
  heartbeat_send_timer_->EnableTimer(kAgentHeartbeatInterval);
}

Status HeartbeatMessageHandler::SendHeartbeatInternal() {
  // HB Code Start.
  if (heartbeat_info_.last_ackd_seq_num < heartbeat_info_.last_sent_seq_num) {
    // Send over the previous request again.
    return nats_conn()->Publish(*last_sent_hb_);
  }

  heartbeat_info_.last_sent_seq_num++;

  last_sent_hb_ = std::make_unique<messages::VizierMessage>();

  auto& req = *last_sent_hb_;
  auto hb = req.mutable_heartbeat();
  ToProto(agent_info()->agent_id, hb->mutable_agent_id());

  // We want to send system clock on the heartbeats.
  auto time_since_epoch = time_source_.SystemTime().time_since_epoch();
  auto time_since_epoch_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_since_epoch);

  hb->set_time(time_since_epoch_ns.count());
  // If we did not get an ack, we try to send the same heartbeat again.
  if (heartbeat_info_.last_ackd_seq_num == heartbeat_info_.last_sent_seq_num) {
    heartbeat_info_.last_sent_seq_num++;
  }
  hb->set_sequence_number(heartbeat_info_.last_sent_seq_num);

  auto* update_info = hb->mutable_update_info();

  ConsumeAgentPIDUpdates(update_info);
  if (agent_info()->capabilities.collects_data() &&
      (!sent_schema_ || relation_info_manager_->has_updates())) {
    sent_schema_ = true;
    relation_info_manager_->AddSchemaToUpdateInfo(update_info);
  }

  // We skip sending the metadata update when there have been no changes.
  auto current_epoch = mds_manager_->metadata_filter()->epoch_id();
  if (last_metadata_epoch_id_ == 0 || last_metadata_epoch_id_ != current_epoch) {
    auto metadata_filter = update_info->mutable_data()->mutable_metadata_info();
    *metadata_filter = mds_manager_->metadata_filter()->ToProto();
    last_metadata_epoch_id_ = current_epoch;
  }

  VLOG(1) << "Sending heartbeat message: " << req.DebugString();
  heartbeat_info_.last_heartbeat_send_time_ = time_source_.MonotonicTime();

  return nats_conn()->Publish(req);
}

void HeartbeatMessageHandler::HeartbeatWatchdog() {
  if (heartbeat_info_.last_ackd_seq_num < heartbeat_info_.last_sent_seq_num) {
    auto diff = time_source_.MonotonicTime() - heartbeat_info_.last_heartbeat_send_time_;
    if (diff > kHeartbeatWaitMillis) {
      LOG(ERROR) << "Still waiting for heartbeat ACK for seq_num="
                 << heartbeat_info_.last_sent_seq_num;
    }
  }
  heartbeat_watchdog_timer_->EnableTimer(kHeartbeatWaitMillis);
}

Status HeartbeatMessageHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  CHECK(msg->has_heartbeat_ack());
  auto ack = msg->heartbeat_ack();
  heartbeat_info_.last_ackd_seq_num = ack.sequence_number();

  auto time_delta = time_source_.MonotonicTime() - heartbeat_info_.last_heartbeat_send_time_;
  heartbeat_latency_moving_average_ =
      kHbLatencyDecay * heartbeat_latency_moving_average_ + (1 - kHbLatencyDecay) * time_delta;

  LOG_EVERY_N(INFO, 60) << absl::StrFormat(
      "Heartbeat ACK latency moving average: %d ms",
      std::chrono::duration_cast<std::chrono::milliseconds>(heartbeat_latency_moving_average_)
          .count());

  if (ack.has_update_info()) {
    CIDRBlock service_cidr;
    Status s = ParseCIDRBlock(ack.update_info().service_cidr(), &service_cidr);
    if (s.ok()) {
      mds_manager_->SetServiceCIDR(service_cidr);
    }

    std::vector<CIDRBlock> pod_cidrs;
    for (const auto& pod_cidr_str : ack.update_info().pod_cidrs()) {
      CIDRBlock pod_cidr;
      Status s = ParseCIDRBlock(pod_cidr_str, &pod_cidr);
      if (s.ok()) {
        pod_cidrs.push_back(std::move(pod_cidr));
      }
    }
    mds_manager_->SetPodCIDR(std::move(pod_cidrs));
  }

  return Status::OK();
}

void HeartbeatMessageHandler::ConsumeAgentPIDUpdates(messages::AgentUpdateInfo* update_info) {
  while (auto pid_event = mds_manager_->GetNextPIDStatusEvent()) {
    switch (pid_event->type) {
      case px::md::PIDStatusEventType::kStarted: {
        auto* ev = static_cast<px::md::PIDStartedEvent*>(pid_event.get());
        ProcessPIDStartedEvent(*ev, update_info);
        break;
      }
      case px::md::PIDStatusEventType::kTerminated: {
        auto* ev = static_cast<px::md::PIDTerminatedEvent*>(pid_event.get());
        ProcessPIDTerminatedEvent(*ev, update_info);
        break;
      }
      default:
        CHECK(0) << "Unknown PID event";
    }
  }
}

void HeartbeatMessageHandler::ProcessPIDStartedEvent(const px::md::PIDStartedEvent& ev,
                                                     messages::AgentUpdateInfo* update_info) {
  auto* process_info = update_info->add_process_created();

  auto upid = ev.pid_info.upid();
  auto* mu_upid = process_info->mutable_upid();
  mu_upid->set_high(absl::Uint128High64(upid.value()));
  mu_upid->set_low(absl::Uint128Low64(upid.value()));

  process_info->set_start_timestamp_ns(ev.pid_info.start_time_ns());
  // Truncate cmdline to fit in the NATS message.
  auto cmdline = ev.pid_info.cmdline();
  if (cmdline.length() > kCmdlineTruncationLimit) {
    cmdline.replace(cmdline.begin() + kCmdlineTruncationLimit, cmdline.end(), kTruncatedMsg);
  }
  process_info->set_cmdline(cmdline);
  process_info->set_cid(ev.pid_info.cid());
}

void HeartbeatMessageHandler::ProcessPIDTerminatedEvent(const px::md::PIDTerminatedEvent& ev,
                                                        messages::AgentUpdateInfo* update_info) {
  auto process_info = update_info->add_process_terminated();
  process_info->set_stop_timestamp_ns(ev.stop_time_ns);

  auto upid = ev.upid;
  // TODO(zasgar): Move this into ToProto function.
  auto* mu_pid = process_info->mutable_upid();
  mu_pid->set_high(absl::Uint128High64(upid.value()));
  mu_pid->set_low(absl::Uint128Low64(upid.value()));
}

Status HeartbeatNackMessageHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  CHECK(msg->has_heartbeat_nack());
  if (msg->heartbeat_nack().reregister()) {
    return reregister_hook_();
  }
  LOG(FATAL) << "Got a heartbeat NACK. Terminating... : " << msg->DebugString();
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
