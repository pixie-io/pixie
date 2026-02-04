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
#include "src/experimental/standalone_pem/tracepoint_manager.h"
#include "src/shared/tracepoint_translation/translation.h"

namespace px {
namespace vizier {
namespace agent {

constexpr auto kUpdateInterval = std::chrono::seconds(1);

TracepointManager::TracepointManager(px::event::Dispatcher* dispatcher,
                                     stirling::Stirling* stirling,
                                     table_store::TableStore* table_store)
    : dispatcher_(dispatcher), stirling_(stirling), table_store_(table_store) {
  // Monitor is the background co-routine that monitors the state of the probes and sends
  // updates to the MDS.
  tracepoint_monitor_timer_ =
      dispatcher_->CreateTimer(std::bind(&TracepointManager::Monitor, this));
  // Kick off the background monitor.
  tracepoint_monitor_timer_->EnableTimer(kUpdateInterval);
}

Status TracepointManager::RegisterTracepoint(
    stirling::dynamic_tracing::ir::logical::TracepointDeployment* program, sole::uuid id) {
  const std::string& name = program->name();
  auto tp =
      std::make_unique<stirling::dynamic_tracing::ir::logical::TracepointDeployment>(*program);
  TracepointInfo info;
  info.name = name;
  info.current_state = statuspb::PENDING_STATE;
  info.expected_state = statuspb::RUNNING_STATE;
  info.last_updated_at = dispatcher_->GetTimeSource().MonotonicTime();

  // Poke Stirling to actually add the tracepoint.
  stirling_->RegisterTracepoint(id, std::move(tp));

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracepoints_[id] = std::move(info);
    tracepoint_name_map_[name] = id;
  }

  // Start a dispatcher to remove the tracepoint @ TTL.
  return Status::OK();
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
  }
  tracepoint_monitor_timer_->EnableTimer(kUpdateInterval);
}

TracepointInfo* TracepointManager::GetTracepoint(std::string name) {
  std::lock_guard<std::mutex> lock(mu_);
  auto pair = tracepoint_name_map_.find(name);
  if (pair == tracepoint_name_map_.end()) {
    return nullptr;
  }

  auto id_pair = tracepoints_.find(pair->second);
  if (id_pair == tracepoints_.end()) {
    return nullptr;
  }

  return &id_pair->second;
}

Status TracepointManager::UpdateSchema(const stirling::stirlingpb::Publish& publish_pb) {
  auto relation_info_vec = ConvertPublishPBToRelationInfo(publish_pb);

  // TODO(zasgar): Failure here can lead to an inconsistent schema state. We should
  // // figure out how to handle this as part of the data model refactor project.
  for (const auto& relation_info : relation_info_vec) {
    table_store_->AddTable(table_store::Table::Create(relation_info.name, relation_info.relation),
                           relation_info.name, relation_info.id);
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
