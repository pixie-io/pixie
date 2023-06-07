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

#include <memory>
#include <string>
#include <utility>

#include <absl/base/internal/spinlock.h>
#include "src/common/system/proc_pid_path.h"
#include "src/shared/metadata/standalone_state_manager.h"

namespace px {
namespace md {

// Returns the list of processes from the proc filesystem. Used by StandaloneContext.
absl::flat_hash_set<md::UPID> ListUPIDs(const std::filesystem::path& proc_path, uint32_t asid) {
  absl::flat_hash_set<md::UPID> pids;
  for (const auto& p : std::filesystem::directory_iterator(proc_path)) {
    uint32_t pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      continue;
    }
    StatusOr<int64_t> pid_start_time = system::GetPIDStartTimeTicks(p.path());
    if (!pid_start_time.ok()) {
      VLOG(1) << absl::Substitute("Could not get PID start time for pid $0. Likely already dead.",
                                  p.path().string());
      continue;
    }
    pids.emplace(asid, pid, pid_start_time.ValueOrDie());
  }
  return pids;
}

std::shared_ptr<const AgentMetadataState>
StandaloneAgentMetadataStateManager::CurrentAgentMetadataState() {
  absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
  return std::const_pointer_cast<const AgentMetadataState>(agent_metadata_state_);
}

Status StandaloneAgentMetadataStateManager::PerformMetadataStateUpdate() {
  // There should never be more than one update, but this just here for safety.
  std::lock_guard<std::mutex> state_update_lock(metadata_state_update_lock_);

  /*
   * Performing a state update involves:
   *   1. Create a copy of the current metadata state.
   *   2. Scan proc for changes.
   *   3. Set current update time and increment the epoch.
   *   4. Replace the current agent_metdata_state_ ptr.
   */
  uint32_t asid = 0;
  uint64_t epoch_id = 0;
  std::shared_ptr<AgentMetadataState> shadow_state;
  {
    absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
    // Copy the current state into the shadow state.
    shadow_state = agent_metadata_state_->CloneToShared();
    epoch_id = agent_metadata_state_->epoch_id();
    asid = agent_metadata_state_->asid();
  }

  int64_t ts = agent_metadata_state_->current_time();
  auto upids = ListUPIDs(px::system::proc_path(), asid);
  for (const md::UPID& up : agent_metadata_state_->upids()) {
    if (!upids.contains(up)) {
      // Pid has been stopped.
      shadow_state->MarkUPIDAsStopped(up, ts);
    }
  }

  px::system::ProcParser proc_parser;
  for (const md::UPID& up : upids) {
    if (shadow_state->GetPIDByUPID(up) == nullptr) {
      // UPID does not exist. Add to the tracker.
      std::string exe_path = proc_parser.GetExePath(up.pid()).ValueOr("");
      std::string cmdline = proc_parser.GetPIDCmdline(up.pid());
      auto pid_info =
          std::make_unique<PIDInfo>(up, std::move(exe_path), std::move(cmdline), CID("unknown"));
      shadow_state->AddUPID(up, std::move(pid_info));
    }
  }
  VLOG(1) << absl::Substitute("Starting update of current MDS, epoch_id=$0", epoch_id);
  VLOG(2) << "Current State: \n" << shadow_state->DebugString(1 /* indent_level */);

  // Increment epoch and update ts.
  ++epoch_id;
  shadow_state->set_epoch_id(epoch_id);
  shadow_state->set_last_update_ts_ns(ts);

  // Set Pod CIDR to be as exclusive as possible, because client-side tracing will be
  // disabled for any addresses that fall in the CIDR.
  px::CIDRBlock pod_cidr;
  std::string pod_cidr_str("0.0.0.1/32");
  PX_RETURN_IF_ERROR(px::ParseCIDRBlock(pod_cidr_str, &pod_cidr));
  shadow_state->k8s_metadata_state()->set_pod_cidrs({pod_cidr});

  {
    absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
    agent_metadata_state_ = std::move(shadow_state);
    shadow_state.reset();
  }

  VLOG(1) << "State Update Complete";
  VLOG(2) << "New MDS State: " << agent_metadata_state_->DebugString();
  return Status::OK();
}

}  // namespace md
}  // namespace px
