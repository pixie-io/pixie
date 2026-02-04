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

#include <string>
#include <utility>

#include "src/carnot/planner/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/common/base/base.h"
#include "src/shared/tracepoint_translation/translation.h"
#include "src/vizier/services/agent/pem/tracepoint_manager.h"

namespace px {
namespace vizier {
namespace agent {

constexpr auto kUpdateInterval = std::chrono::seconds(2);

TracepointManager::TracepointManager(px::event::Dispatcher* dispatcher, Info* agent_info,
                                     Manager::VizierNATSConnector* nats_conn,
                                     stirling::Stirling* stirling,
                                     table_store::TableStore* table_store,
                                     RelationInfoManager* relation_info_manager)
    : MessageHandler(dispatcher, agent_info, nats_conn),
      dispatcher_(dispatcher),
      nats_conn_(nats_conn),
      stirling_(stirling),
      table_store_(table_store),
      relation_info_manager_(relation_info_manager) {
  // Monitor is the background co-routine that monitors the state of the probes and sends
  // updates to the MDS.
  tracepoint_monitor_timer_ =
      dispatcher_->CreateTimer(std::bind(&TracepointManager::Monitor, this));
  // Kick off the background monitor.
  tracepoint_monitor_timer_->EnableTimer(kUpdateInterval);
  PX_UNUSED(nats_conn_);
}

Status TracepointManager::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  // The main purpose of handle message is to update the local state based on updates
  // from the MDS.
  if (!msg->has_tracepoint_message()) {
    return error::InvalidArgument("Can only handle probe requests");
  }

  const messages::TracepointMessage& tracepoint = msg->tracepoint_message();
  switch (tracepoint.msg_case()) {
    case messages::TracepointMessage::kRegisterTracepointRequest: {
      return HandleRegisterTracepointRequest(tracepoint.register_tracepoint_request());
    }
    case messages::TracepointMessage::kRemoveTracepointRequest: {
      return HandleRemoveTracepointRequest(tracepoint.remove_tracepoint_request());
    }
    default:
      LOG(ERROR) << "Unknown message type: " << tracepoint.msg_case() << " skipping";
  }
  return Status::OK();
}

Status TracepointManager::HandleRegisterTracepointRequest(
    const messages::RegisterTracepointRequest& req) {
  const std::string& name = req.tracepoint_deployment().name();
  PX_ASSIGN_OR_RETURN(auto id, ParseUUID(req.id()));
  auto program = std::make_unique<stirling::dynamic_tracing::ir::logical::TracepointDeployment>();
  ::px::tracepoint::ConvertPlannerTracepointToStirlingTracepoint(req.tracepoint_deployment(),
                                                                 program.get());

  TracepointInfo info;
  info.name = name;
  info.current_state = statuspb::PENDING_STATE;
  info.expected_state = statuspb::RUNNING_STATE;
  info.last_updated_at = dispatcher_->GetTimeSource().MonotonicTime();

  // Poke Stirling to actually add the tracepoint.
  stirling_->RegisterTracepoint(id, std::move(program));

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracepoints_[id] = std::move(info);
  }
  return Status::OK();
}

Status TracepointManager::HandleRemoveTracepointRequest(
    const messages::RemoveTracepointRequest& req) {
  PX_ASSIGN_OR_RETURN(auto id, ParseUUID(req.id()));
  std::lock_guard<std::mutex> lock(mu_);

  auto it = tracepoints_.find(id);
  if (it == tracepoints_.end()) {
    return error::NotFound("Tracepoint with ID: $0, not found", id.str());
  }

  it->second.expected_state = statuspb::TERMINATED_STATE;
  return stirling_->RemoveTracepoint(id);
}

std::string TracepointManager::DebugString() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::stringstream ss;
  auto now = std::chrono::steady_clock::now();
  ss << absl::Substitute("Tracepoint Manager Debug State:\n");
  ss << absl::Substitute("ID\tNAME\tCURRENT_STATE\tEXPECTED_STATE\tlast_updated\n");
  for (const auto& [id, tracepoint] : tracepoints_) {
    ss << absl::Substitute(
        "$0\t$1\t$2\t$3\t$4 seconds\n", id.str(), tracepoint.name,
        statuspb::LifeCycleState_Name(tracepoint.current_state),
        statuspb::LifeCycleState_Name(tracepoint.expected_state),
        std::chrono::duration_cast<std::chrono::seconds>(now - tracepoint.last_updated_at).count());
  }
  return ss.str();
}

void TracepointManager::Monitor() {
  std::lock_guard<std::mutex> lock(mu_);

  for (auto& [id, tracepoint] : tracepoints_) {
    auto s_or_publish = stirling_->GetTracepointInfo(id);
    statuspb::LifeCycleState current_state;
    // Get the latest current state according to stirling.
    if (s_or_publish.ok()) {
      current_state = statuspb::RUNNING_STATE;
    } else {
      switch (s_or_publish.code()) {
        case statuspb::FAILED_PRECONDITION:
          // Means the binary has not been found.
          current_state = statuspb::FAILED_STATE;
          break;
        case statuspb::RESOURCE_UNAVAILABLE:
          current_state = statuspb::PENDING_STATE;
          break;
        case statuspb::NOT_FOUND:
          // Means we didn't actually find the probe. If we requested termination,
          // it's because the probe has been removed.
          current_state = (tracepoint.expected_state == statuspb::TERMINATED_STATE)
                              ? statuspb::TERMINATED_STATE
                              : statuspb::UNKNOWN_STATE;
          break;
        default:
          current_state = statuspb::FAILED_STATE;
          break;
      }
    }

    if (current_state != statuspb::RUNNING_STATE &&
        tracepoint.expected_state == statuspb::TERMINATED_STATE) {
      current_state = statuspb::TERMINATED_STATE;
    }

    if (current_state == tracepoint.current_state) {
      // No state transition, nothing to do.
      continue;
    }

    // The following transitions are legal:
    // 1. Pending -> Terminated: Probe is stopped before starting.
    // 2. Pending -> Running : Probe starts up.
    // 3. Running -> Terminated: Probe is stopped.
    // 4. Running -> Failed: Probe got dettached because binary died.
    // 5. Failed -> Running: Probe started up because binary came back to life.
    //
    // In all cases we basically inform the MDS.
    // In the cases where we transition to running, we need to update the schemas.

    Status probe_status = Status::OK();
    LOG(INFO) << absl::Substitute("Tracepoint[$0]::$1 has transitioned $2 -> $3", id.str(),
                                  tracepoint.name,
                                  statuspb::LifeCycleState_Name(tracepoint.current_state),
                                  statuspb::LifeCycleState_Name(current_state));
    // Check if running now, then update the schema.
    if (current_state == statuspb::RUNNING_STATE) {
      // We must have just transitioned into running. We try to apply the new schema.
      // If it fails we will trigger an error and report that to MDS.
      auto publish_pb = s_or_publish.ConsumeValueOrDie();
      auto s = UpdateSchema(publish_pb);
      if (!s.ok()) {
        current_state = statuspb::FAILED_STATE;
        probe_status = s;
      }
    } else {
      probe_status = s_or_publish.status();
    }

    tracepoint.current_state = current_state;
    // Update MDS with the latest status.
    px::vizier::messages::VizierMessage msg;
    auto tracepoint_msg = msg.mutable_tracepoint_message();
    auto update_msg = tracepoint_msg->mutable_tracepoint_info_update();
    ToProto(agent_info()->agent_id, update_msg->mutable_agent_id());
    ToProto(id, update_msg->mutable_id());
    update_msg->set_state(tracepoint.current_state);
    probe_status.ToProto(update_msg->mutable_status());
    auto s = nats_conn_->Publish(msg);
    if (!s.ok()) {
      LOG(ERROR) << "Failed to update nats";
    }
  }
  tracepoint_monitor_timer_->EnableTimer(kUpdateInterval);
}

Status TracepointManager::UpdateSchema(const stirling::stirlingpb::Publish& publish_pb) {
  auto relation_info_vec = ConvertPublishPBToRelationInfo(publish_pb);

  // TODO(zasgar): Failure here can lead to an inconsistent schema state. We should
  // figure out how to handle this as part of the data model refactor project.
  for (const auto& relation_info : relation_info_vec) {
    if (!relation_info_manager_->HasRelation(relation_info.name)) {
      table_store_->AddTable(table_store::Table::Create(relation_info.name, relation_info.relation),
                             relation_info.name, relation_info.id);
      PX_RETURN_IF_ERROR(relation_info_manager_->AddRelationInfo(relation_info));
    } else {
      if (relation_info.relation != table_store_->GetTable(relation_info.name)->GetRelation()) {
        return error::Internal(
            "Tracepoint is not compatible with the schema of the specified output table. "
            "[table_name=$0]",
            relation_info.name);
      }
      PX_RETURN_IF_ERROR(table_store_->AddTableAlias(relation_info.id, relation_info.name));
    }
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
