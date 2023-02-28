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

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <prometheus/counter.h>
#include <prometheus/gauge.h>

#include "src/stirling/source_connectors/socket_tracer/conn_tracker.h"
#include "src/stirling/utils/obj_pool.h"
#include "src/stirling/utils/stat_counter.h"

DECLARE_double(stirling_conn_tracker_cleanup_threshold);

namespace px {
namespace stirling {

using ConnTrackerPool = ObjPool<ConnTracker>;

/**
 * ConnTrackersGenerations is a container of tracker generations,
 * where a generation is identified by the timestamp ID (TSID).
 *
 * It automatically handles marking older generations for death.
 */
class ConnTrackerGenerations {
 public:
  /**
   * Get ConnTracker by TSID, or return a new one if the TSID does not exist.
   *
   * @return The pointer to the conn_tracker and whether the tracker was newly created.
   */
  std::pair<ConnTracker*, bool> GetOrCreate(conn_id_t conn_id, ConnTrackerPool* tracker_pool);

  /**
   * Return true if a ConnTracker created at the specified time stamp exists.
   */
  bool Contains(uint64_t tsid) const;

  /**
   * Returns the oldest tracker, or error if the oldest tracker either has been destroyed
   * or is ReadyForDestruction().
   */
  StatusOr<const ConnTracker*> GetActive() const;

  const auto& generations() const { return generations_; }
  bool empty() const { return generations_.empty(); }

  /**
   * Removes all trackers that are ReadyForDestruction().
   * Removed trackers are pushed into the tracker pool for recycling.
   */
  int CleanupGenerations(ConnTrackerPool* tracker_pool);

 private:
  // A map of TSID to ConnTrackers.
  absl::flat_hash_map<uint64_t, std::unique_ptr<ConnTracker>> generations_;

  // Keep a pointer to the ConnTracker generation with the highest TSID.
  ConnTracker* oldest_generation_ = nullptr;
};

/**
 * ConnTrackersManager is a container that keeps track of all ConnTrackers.
 * Interface designed for two primary operations:
 *  1) Insertion of events indexed by conn_id (PID+FD+TSID) as they arrive from BPF.
 *  2) Iteration through trackers by protocols.
 */
class ConnTrackersManager {
 public:
  enum class StatKey {
    kTotal,
    kReadyForDestruction,

    kCreated,
    kDestroyed,
    kDestroyedGens,
  };

  ConnTrackersManager();

  /**
   * Get a connection tracker for the specified conn_id. If a tracker does not exist,
   * one will be created and returned.
   */
  ConnTracker& GetOrCreateConnTracker(struct conn_id_t conn_id);

  const std::list<ConnTracker*>& active_trackers() const { return active_trackers_; }

  /**
   * Returns the latest generation of a connection tracker for the given pid and fd.
   * If there is no tracker for {pid, fd}, returns error::NotFound.
   */
  StatusOr<const ConnTracker*> GetConnTracker(uint32_t pid, int32_t fd) const;

  /**
   * Deletes trackers that are ReadyForDestruction().
   * Call this only after accumulating enough trackers to clean-up, to avoid the performance
   * impact of scanning through all trackers every iteration.
   */
  void CleanupTrackers();

  /**
   * Returns extensive debug information about the connection trackers.
   */
  std::string DebugInfo() const;

  /**
   * Computes the count of ConnTracker objects for each protocol and stores them into stats_.
   */
  void ComputeProtocolStats();

  /**
   * Returns a string representing the stats of ConnTracker objects.
   */
  std::string StatsString() const;

 private:
  // Simple consistency DCHECKs meant for enforcing invariants.
  void DebugChecks() const;

  // A map from conn_id (PID+FD+TSID) to tracker. This is for easy update on BPF events.
  // Structured as two nested maps to be explicit about "generations" of trackers per PID+FD.
  // Key is {PID, FD} for outer map, and tsid for inner map.
  absl::flat_hash_map<uint64_t, ConnTrackerGenerations> conn_id_tracker_generations_;

  std::list<ConnTracker*> active_trackers_;

  // A pool of unused trackers that can be recycled.
  // This is useful for avoiding memory reallocations.
  ConnTrackerPool trackers_pool_;

  // Records statistics of ConnTracker for reporting and consistency check.
  utils::StatCounter<StatKey> stats_;
  utils::StatCounter<traffic_protocol_t> protocol_stats_;

  prometheus::Counter& conn_tracker_created_;
  prometheus::Counter& conn_tracker_destroyed_;
  prometheus::Counter& destroyed_gens_;
};

}  // namespace stirling
}  // namespace px
