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
#include "src/common/metrics/metrics.h"

DEFINE_double(
    stirling_conn_tracker_cleanup_threshold, 0.2,
    "Percentage of trackers that are ready for destruction that will trigger a memory cleanup");

namespace px {
namespace stirling {

//-----------------------------------------------------------------------------
// ConnTrackerGenerations
//-----------------------------------------------------------------------------

std::pair<ConnTracker*, bool> ConnTrackerGenerations::GetOrCreate(conn_id_t conn_id,
                                                                  ConnTrackerPool* tracker_pool) {
  std::unique_ptr<ConnTracker>& conn_tracker_ptr = generations_[conn_id.tsid];
  bool created = false;

  if (conn_tracker_ptr == nullptr) {
    conn_tracker_ptr = tracker_pool->Pop();
    created = true;
    conn_tracker_ptr->SetConnID(conn_id);

    // If there is a another generation for this conn map key,
    // one of them needs to be marked for death.
    if (oldest_generation_ != nullptr) {
      // If the inserted conn_tracker is not the last generation, then mark it for death.
      // This can happen because the events draining from the perf buffers are not ordered.
      if (conn_id.tsid < oldest_generation_->conn_id().tsid) {
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

uint64_t GetConnMapKey(uint32_t pid, int32_t fd) { return (static_cast<uint64_t>(pid) << 32) | fd; }

}  // namespace

ConnTrackersManager::ConnTrackersManager()
    : trackers_pool_(kMaxConnTrackerPoolSize),
      conn_tracker_created_(BuildCounter("conn_tracker_created",
                                         "Counter that tracks when a conn tracker is created")),
      conn_tracker_destroyed_(BuildCounter("conn_tracker_destroyed",
                                           "Counter that tracks when a conn tracker is destroyed")),
      destroyed_gens_(BuildCounter(
          "destroyed_gens", "Counter that tracks how many destroyed generations have occurred")) {}

ConnTracker& ConnTrackersManager::GetOrCreateConnTracker(struct conn_id_t conn_id) {
  const uint64_t conn_map_key = GetConnMapKey(conn_id.upid.pid, conn_id.fd);
  DCHECK_NE(conn_map_key, 0U) << "Connection map key cannot be 0, pid must be wrong";

  ConnTrackerGenerations& conn_trackers = conn_id_tracker_generations_[conn_map_key];
  auto [conn_tracker_ptr, created] = conn_trackers.GetOrCreate(conn_id, &trackers_pool_);

  if (created) {
    active_trackers_.push_back(conn_tracker_ptr);
    conn_tracker_ptr->manager_ = this;

    stats_.Increment(StatKey::kTotal);
    stats_.Increment(StatKey::kCreated);
    conn_tracker_created_.Increment();
  }

  DebugChecks();
  return *conn_tracker_ptr;
}

StatusOr<const ConnTracker*> ConnTrackersManager::GetConnTracker(uint32_t pid, int32_t fd) const {
  const uint64_t conn_map_key = GetConnMapKey(pid, fd);

  auto tracker_set_it = conn_id_tracker_generations_.find(conn_map_key);
  if (tracker_set_it == conn_id_tracker_generations_.end()) {
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
  {
    auto iter = active_trackers_.begin();
    while (iter != active_trackers_.end()) {
      const auto& tracker = *iter;
      if (tracker->ReadyForDestruction()) {
        active_trackers_.erase(iter++);

        stats_.Increment(StatKey::kReadyForDestruction);
      } else {
        ++iter;
      }
    }
  }

  // As a performance optimization, we only clean up trackers once we reach a certain threshold
  // of trackers that are ready for destruction.
  // Trade-off is just how quickly we release memory and BPF map entries.
  double percent_destroyable =
      1.0 * stats_.Get(StatKey::kReadyForDestruction) / stats_.Get(StatKey::kTotal);
  if (percent_destroyable > FLAGS_stirling_conn_tracker_cleanup_threshold) {
    // Outer loop iterates through tracker sets (keyed by PID+FD),
    // while inner loop iterates through generations of trackers for that PID+FD pair.
    auto iter = conn_id_tracker_generations_.begin();
    while (iter != conn_id_tracker_generations_.end()) {
      auto& tracker_generations = iter->second;

      int num_erased = tracker_generations.CleanupGenerations(&trackers_pool_);

      stats_.Decrement(StatKey::kTotal, num_erased);
      stats_.Decrement(StatKey::kReadyForDestruction, num_erased);
      stats_.Increment(StatKey::kDestroyed, num_erased);

      conn_tracker_destroyed_.Increment(num_erased);

      if (tracker_generations.empty()) {
        conn_id_tracker_generations_.erase(iter++);

        stats_.Increment(StatKey::kDestroyedGens);
        destroyed_gens_.Increment();
      } else {
        ++iter;
      }
    }
  }

  DebugChecks();
}

void ConnTrackersManager::DebugChecks() const {
  DCHECK_EQ(stats_.Get(StatKey::kTotal), static_cast<int64_t>(active_trackers_.size()) +
                                             stats_.Get(StatKey::kReadyForDestruction));
}

std::string ConnTrackersManager::DebugInfo() const {
  std::string out;

  absl::StrAppend(&out, "ConnTracker count statistics: ", StatsString(),
                  "\nDetailed statistics of individual ConnTracker:\n");

  for (const auto& tracker : active_trackers_) {
    absl::StrAppend(&out, absl::Substitute("  conn_tracker=$0 zombie=$1 ready_for_destruction=$2\n",
                                           tracker->ToString(), tracker->IsZombie(),
                                           tracker->ReadyForDestruction()));
  }

  return out;
}

std::string ConnTrackersManager::StatsString() const {
  return absl::StrCat(stats_.Print(), protocol_stats_.Print());
}

void ConnTrackersManager::ComputeProtocolStats() {
  absl::flat_hash_map<traffic_protocol_t, int> protocol_count;
  for (const auto* tracker : active_trackers_) {
    ++protocol_count[tracker->protocol()];
  }
  for (auto protocol : magic_enum::enum_values<traffic_protocol_t>()) {
    protocol_stats_.Reset(protocol);
    auto iter = protocol_count.find(protocol);
    if (iter != protocol_count.end()) {
      protocol_stats_.Increment(protocol, iter->second);
    }
  }
}

}  // namespace stirling
}  // namespace px
