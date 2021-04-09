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

#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"

namespace px {
namespace stirling {

//-----------------------------------------------------------------------------
// ConnTrackerGenerations
//-----------------------------------------------------------------------------

std::pair<ConnTracker*, bool> ConnTrackerGenerations::GetOrCreate(uint64_t tsid,
                                                                  ConnTrackerPool* tracker_pool) {
  std::unique_ptr<ConnTracker>& conn_tracker_ptr = generations_[tsid];
  bool created = false;

  // If conn_trackers[conn_id.tsid] does not exist, a new one will be created in the map.
  // New trackers always have TSID == 0, while BPF should never generate a TSID of zero,
  // so we use this as a way of detecting new trackers.
  if (conn_tracker_ptr == nullptr) {
    conn_tracker_ptr = tracker_pool->Pop();
    created = true;

    // If there is a another generation for this conn map key,
    // one of them needs to be marked for death.
    if (oldest_generation_ != nullptr) {
      // If the inserted conn_tracker is not the last generation, then mark it for death.
      // This can happen because the events draining from the perf buffers are not ordered.
      if (tsid < oldest_generation_->conn_id().tsid) {
        VLOG(1) << "Marking for death because not last generation.";
        conn_tracker_ptr->MarkForDeath();
      } else {
        // New tracker was the last, so the previous last should be marked for death.
        VLOG(1) << "Marking previous generation for death.";
        oldest_generation_->MarkForDeath();
        oldest_generation_ = conn_tracker_ptr.get();
      }
    } else {
      oldest_generation_ = conn_tracker_ptr.get();
    }
  }

  return std::make_pair(conn_tracker_ptr.get(), created);
}

bool ConnTrackerGenerations::Contains(uint64_t tsid) const { return generations_.contains(tsid); }

StatusOr<const ConnTracker*> ConnTrackerGenerations::GetActive() const {
  // Don't return trackers that are destroyed or about to be destroyed.
  if (oldest_generation_ == nullptr || oldest_generation_->ReadyForDestruction()) {
    return error::NotFound("No active connection trackers found");
  }

  return oldest_generation_;
}

int ConnTrackerGenerations::CleanupGenerations(ConnTrackerPool* tracker_pool) {
  int num_erased = 0;

  auto iter = generations_.begin();
  while (iter != generations_.end()) {
    auto& tracker = iter->second;

    // Remove any trackers that are no longer required.
    if (tracker->ReadyForDestruction()) {
      if (tracker.get() == oldest_generation_) {
        oldest_generation_ = nullptr;
      }

      tracker_pool->Recycle(std::move(tracker));

      generations_.erase(iter++);
      ++num_erased;
    } else {
      ++iter;
    }
  }

  return num_erased;
}

//-----------------------------------------------------------------------------
// ConnTrackersManager
//-----------------------------------------------------------------------------

namespace {

constexpr size_t kMaxConnTrackerPoolSize = 2048;

uint64_t GetConnMapKey(uint32_t pid, uint32_t fd) {
  return (static_cast<uint64_t>(pid) << 32) | fd;
}

}  // namespace

ConnTrackersManager::ConnTrackersManager() : trackers_pool_(kMaxConnTrackerPoolSize) {}

void ConnTrackersManager::UpdateProtocol(px::stirling::ConnTracker* tracker,
                                         std::optional<TrafficProtocol> old_protocol) {
  // If the tracker is ReadyForDestruction(), then it should not be a member of any protocol list.
  if (tracker->ReadyForDestruction()) {
    // Since it is not part of any protocol list, it should not have a back pointer to one.
    DCHECK(!tracker->back_pointer_.has_value());
    return;
  }

  if (old_protocol.has_value()) {
    // If an old protocol is specified, then the tracker should also have been set up
    // with a back pointer to its list.
    DCHECK(tracker->back_pointer_.has_value());

    if (old_protocol.value() == tracker->traffic_class().protocol) {
      // Didn't really move, so nothing to update.
      return;
    }

    // Remove tracker from previous list.
    conn_trackers_by_protocol_[old_protocol.value()].erase(tracker->back_pointer_.value());
    --num_trackers_in_lists_;
  } else {
    // If no old protocol is specified, the the tracker should not be in any list.
    // Currently, this should only be possible on initialization of a new tracker.
    DCHECK(!tracker->back_pointer_.has_value());
  }

  // Add tracker to new list based on its current protocol.
  conn_trackers_by_protocol_[tracker->traffic_class().protocol].push_back(tracker);
  tracker->back_pointer_ = --conn_trackers_by_protocol_[tracker->traffic_class().protocol].end();
  ++num_trackers_in_lists_;

  DebugChecks();
}

ConnTracker& ConnTrackersManager::GetOrCreateConnTracker(struct conn_id_t conn_id) {
  const uint64_t conn_map_key = GetConnMapKey(conn_id.upid.pid, conn_id.fd);
  DCHECK_NE(conn_map_key, 0) << "Connection map key cannot be 0, pid must be wrong";

  ConnTrackerGenerations& conn_trackers = conn_id_to_conn_tracker_generations_[conn_map_key];
  auto [conn_tracker_ptr, created] = conn_trackers.GetOrCreate(conn_id.tsid, &trackers_pool_);

  if (created) {
    ++num_trackers_;
    conn_tracker_ptr->manager_ = this;
    UpdateProtocol(conn_tracker_ptr, {});
  }

  DebugChecks();
  return *conn_tracker_ptr;
}

StatusOr<const ConnTracker*> ConnTrackersManager::GetConnTracker(uint32_t pid, uint32_t fd) const {
  const uint64_t conn_map_key = GetConnMapKey(pid, fd);

  auto tracker_set_it = conn_id_to_conn_tracker_generations_.find(conn_map_key);
  if (tracker_set_it == conn_id_to_conn_tracker_generations_.end()) {
    return error::NotFound("Could not find the tracker with pid=$0 fd=$1.", pid, fd);
  }

  const auto& tracker_generations = tracker_set_it->second;

  if (tracker_generations.empty()) {
    DCHECK(false) << "Map entry with no tracker generations should never exist. Entry should be "
                     "deleted in such cases.";
    return error::Internal("Corrupted state: map entry with no tracker generations.");
  }

  // Return last connection.
  return tracker_generations.GetActive();
}

void ConnTrackersManager::CleanupTrackers() {
  // Outer loop iterates through tracker sets (keyed by PID+FD),
  // while inner loop iterates through generations of trackers for that PID+FD pair.
  auto iter = conn_id_to_conn_tracker_generations_.begin();
  while (iter != conn_id_to_conn_tracker_generations_.end()) {
    auto& tracker_generations = iter->second;

    int num_erased = tracker_generations.CleanupGenerations(&trackers_pool_);

    num_trackers_ -= num_erased;
    num_trackers_ready_for_destruction_ -= num_erased;

    if (tracker_generations.empty()) {
      conn_id_to_conn_tracker_generations_.erase(iter++);
    } else {
      ++iter;
    }
  }

  DebugChecks();
}

Status ConnTrackersManager::TestOnlyCheckConsistency() const {
  // A set used for looking for duplicate trackers.
  std::set<ConnTracker*> trackers_set;

  for (const auto& [protocol, conn_trackers_list] : conn_trackers_by_protocol_) {
    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnTracker* tracker = *iter;

      // Check that tracker exists (i.e. that the pointer is valid).
      // If the pointer is not valid, this will likely cause a crash or ASAN error.
      const uint64_t conn_map_key =
          GetConnMapKey(tracker->conn_id().upid.pid, tracker->conn_id().fd);
      DCHECK_NE(conn_map_key, 0) << "Connection map key cannot be 0, pid must be wrong";
      auto tracker_set_it = conn_id_to_conn_tracker_generations_.find(conn_map_key);
      if (tracker_set_it == conn_id_to_conn_tracker_generations_.end()) {
        return error::Internal("Tracker $0 in the protocol lists not found.",
                               ToString(tracker->conn_id()));
      }

      const auto& tracker_generations = tracker_set_it->second;
      if (!tracker_generations.Contains(tracker->conn_id().tsid)) {
        return error::Internal("Tracker $0 in the protocol lists not found.",
                               ToString(tracker->conn_id()));
      }

      // Check that the pointer only shows up once across all lists.
      auto [unused, inserted] = trackers_set.insert(tracker);
      if (!inserted) {
        return error::Internal("Tracker $0 found in two lists.", ToString(tracker->conn_id()));
      }
    }
  }

  return Status::OK();
}

void ConnTrackersManager::DebugChecks() const {
  DCHECK_EQ(num_trackers_, num_trackers_in_lists_ + num_trackers_ready_for_destruction_);
}

std::string ConnTrackersManager::DebugInfo() const {
  std::string out;

  for (const auto& [protocol, conn_trackers_list] : conn_trackers_by_protocol_) {
    absl::StrAppend(&out,
                    absl::Substitute("protocol=$0 num_trackers=$1", magic_enum::enum_name(protocol),
                                     conn_trackers_list.size()));

    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnTracker* tracker = *iter;

      absl::StrAppend(&out,
                      absl::Substitute(
                          "   conn_id=$0 protocol=$1 state=$2 zombie=$3 ready_for_destruction=$4\n",
                          ToString(tracker->conn_id()),
                          magic_enum::enum_name(tracker->traffic_class().protocol),
                          magic_enum::enum_name(tracker->state()), tracker->IsZombie(),
                          tracker->ReadyForDestruction()));
    }
  }

  return out;
}

//-----------------------------------------------------------------------------
// TrackersListIterator
//-----------------------------------------------------------------------------

ConnTrackersManager::TrackersList::TrackersListIterator::TrackersListIterator(
    std::list<ConnTracker*>* trackers, std::list<ConnTracker*>::iterator iter,
    ConnTrackersManager* conn_trackers_manager)
    : trackers_(trackers), iter_(iter), conn_trackers_manager_(conn_trackers_manager) {}

bool ConnTrackersManager::TrackersList::TrackersListIterator::operator!=(
    const TrackersListIterator& other) {
  return other.iter_ != this->iter_;
}

ConnTracker* ConnTrackersManager::TrackersList::TrackersListIterator::operator*() {
  ConnTracker* tracker = *iter_;
  // Since a tracker can only become ready for destruction in a previous iteration via operator++,
  // and because operator++ would remove such trackers,  we don't expect to see any trackers are
  // ReadyForDestruction here.
  DCHECK(!tracker->ReadyForDestruction());
  return tracker;
}

ConnTrackersManager::TrackersList::TrackersListIterator
ConnTrackersManager::TrackersList::TrackersListIterator::operator++() {
  if ((*iter_)->ReadyForDestruction()) {
    (*iter_)->back_pointer_.reset();
    trackers_->erase(iter_++);
    --conn_trackers_manager_->num_trackers_in_lists_;
    ++conn_trackers_manager_->num_trackers_ready_for_destruction_;
  } else {
    ++iter_;
  }

  return *this;
}

}  // namespace stirling
}  // namespace px
