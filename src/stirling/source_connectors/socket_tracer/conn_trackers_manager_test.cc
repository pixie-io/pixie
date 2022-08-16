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

#include <random>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"

namespace px {
namespace stirling {

using ::testing::HasSubstr;
using ::testing::StrEq;

class ConnTrackersManagerTest : public ::testing::Test {
 protected:
  ConnTrackersManagerTest() : rng_(37), probability_dist_(0.0, 1.0) {}

  ConnTrackersManager trackers_mgr_;

  std::default_random_engine rng_;
  std::uniform_real_distribution<double> probability_dist_;

  void CleanupTrackers() {
    VLOG(1) << "CleanupTrackers";
    trackers_mgr_.CleanupTrackers();
  }

  void TransferStreamsProxy(double mark_for_death_probability, int death_countdown) {
    for (auto& tracker : trackers_mgr_.active_trackers()) {
      if (probability_dist_(rng_) < mark_for_death_probability) {
        tracker->MarkForDeath(death_countdown);
      }
    }
  }

  void TrackerEvent(struct conn_id_t conn_id, traffic_protocol_t protocol) {
    VLOG(1) << "TrackerEvent";
    ConnTracker& tracker = trackers_mgr_.GetOrCreateConnTracker(conn_id);
    tracker.SetProtocol(protocol, "for testing");
  }
};

// This is a stress on ConnTrackersManager.
// Each iteration, a different action is taken, and the consistency of the structure is checked.
// The test also relies on the ConnTrackersManager internal checks (e.g. DebugChecks()).
// ASAN runs can also identify issues while being stressed.
TEST_F(ConnTrackersManagerTest, Fuzz) {
  constexpr int kIters = 1000000;
  std::uniform_int_distribution<int> death_countdown_dist(0, 5);
  std::uniform_int_distribution<int> pid_dist(1, 5);
  std::uniform_int_distribution<int> tsid_dist(1, 2);
  std::uniform_int_distribution<int> protocol_dist(0, kNumProtocols - 1);

  for (int i = 0; i < kIters; ++i) {
    auto protocol = magic_enum::enum_cast<traffic_protocol_t>(protocol_dist(rng_));
    CHECK(protocol.has_value());

    // Randomly pick an action to take.
    //  1) TrackerEvent: proxy of a single BPF event from PollPerfBuffers that could change the
    //  ConnTracker state (including protocol). 2) TransferStreamsProxy: proxy of
    //  TransferStreams(), which processes all the ConnTrackers for a given protocol.
    //      - This can result in a tracker getting into the ReadyForDestruction state.
    //  3) CleanupTrackers: proxy of CleanupTrackers(), which runs periodically in Stirling.
    // This is a proxy of what happens in a real Stirling implementation.
    double x = probability_dist_(rng_);
    if (x < 0.80) {
      uint32_t pid = pid_dist(rng_);
      int32_t fd = 1;
      uint64_t tsid = tsid_dist(rng_);

      struct conn_id_t conn_id = {{{pid}, 0}, fd, tsid};
      TrackerEvent(conn_id, protocol.value());
    } else if (x < 0.95) {
      int death_countdown = death_countdown_dist(rng_);
      double mark_for_death_prob = probability_dist_(rng_);
      TransferStreamsProxy(mark_for_death_prob, death_countdown);
    } else {
      CleanupTrackers();
    }
  }
}

// Tests that the DebugInfo() returns expected text.
TEST_F(ConnTrackersManagerTest, DebugInfo) {
  struct conn_id_t conn_id = {};

  conn_id.upid.pid = 1;
  conn_id.upid.start_time_ticks = 1;
  conn_id.fd = 1;
  conn_id.tsid = 1;

  trackers_mgr_.GetOrCreateConnTracker(conn_id);
  std::string debug_info = trackers_mgr_.DebugInfo();
  EXPECT_THAT(debug_info, HasSubstr("ConnTracker count statistics: kTotal=1 kReadyForDestruction=0 "
                                    "kCreated=1 kDestroyed=0 kDestroyedGens=0"));
  EXPECT_THAT(debug_info, HasSubstr("conn_tracker=conn_id=[upid=1:1 fd=1 gen=1]"));
}

class ConnTrackerGenerationsTest : public ::testing::Test {
 protected:
  ConnTrackerGenerationsTest() : tracker_pool(1024) {
    FLAGS_stirling_check_proc_for_conn_close = false;
  }

  std::pair<ConnTracker*, bool> GetOrCreateTracker(uint64_t tsid) {
    struct conn_id_t conn_id = {};
    conn_id.upid.pid = 1;
    conn_id.upid.start_time_ticks = 1;
    conn_id.fd = 1;
    conn_id.tsid = tsid;

    return tracker_gens_.GetOrCreate(conn_id, &tracker_pool);
  }

  int CleanupTrackers() {
    // Simulate elapsed iterations, which cause trackers to become ReadyForDestruction().
    for (auto& [tsid, tracker] : tracker_gens_.generations()) {
      for (int i = 0; i < ConnTracker::kDeathCountdownIters; ++i) {
        tracker->IterationPostTick();
      }
      tracker->MarkFinalConnStatsReported();
    }

    return tracker_gens_.CleanupGenerations(&tracker_pool);
  }

  ConnTrackerGenerations tracker_gens_;
  ConnTrackerPool tracker_pool;
};

TEST_F(ConnTrackerGenerationsTest, Basic) {
  ASSERT_TRUE(tracker_gens_.empty());
  ASSERT_FALSE(tracker_gens_.Contains(1));

  auto [tracker1, created1] = GetOrCreateTracker(1);
  ASSERT_TRUE(tracker1 != nullptr);
  ASSERT_TRUE(created1);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_TRUE(tracker_gens_.Contains(1));
  ASSERT_FALSE(tracker_gens_.Contains(2));
  ASSERT_FALSE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker1);

  auto [tracker3, created3] = GetOrCreateTracker(3);
  ASSERT_TRUE(tracker3 != nullptr);
  ASSERT_TRUE(created3);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_TRUE(tracker_gens_.Contains(1));
  ASSERT_FALSE(tracker_gens_.Contains(2));
  ASSERT_TRUE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker3);

  auto [tracker2, created2] = GetOrCreateTracker(2);
  ASSERT_TRUE(tracker2 != nullptr);
  ASSERT_TRUE(created2);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_TRUE(tracker_gens_.Contains(1));
  ASSERT_TRUE(tracker_gens_.Contains(2));
  ASSERT_TRUE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker3);

  auto [tracker1b, created1b] = GetOrCreateTracker(1);
  ASSERT_EQ(tracker1b, tracker1);
  ASSERT_FALSE(created1b);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_TRUE(tracker_gens_.Contains(1));
  ASSERT_TRUE(tracker_gens_.Contains(2));
  ASSERT_TRUE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker3);

  auto [tracker2b, created2b] = GetOrCreateTracker(2);
  ASSERT_EQ(tracker2b, tracker2);
  ASSERT_FALSE(created2b);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_TRUE(tracker_gens_.Contains(1));
  ASSERT_TRUE(tracker_gens_.Contains(2));
  ASSERT_TRUE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker3);

  int num_erased1 = CleanupTrackers();
  ASSERT_EQ(num_erased1, 2);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_FALSE(tracker_gens_.Contains(1));
  ASSERT_FALSE(tracker_gens_.Contains(2));
  ASSERT_TRUE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker3);

  int num_erased2 = CleanupTrackers();
  ASSERT_EQ(num_erased2, 0);
  ASSERT_FALSE(tracker_gens_.empty());
  ASSERT_FALSE(tracker_gens_.Contains(1));
  ASSERT_FALSE(tracker_gens_.Contains(2));
  ASSERT_TRUE(tracker_gens_.Contains(3));
  ASSERT_OK_AND_EQ(tracker_gens_.GetActive(), tracker3);

  tracker3->MarkForDeath();
  int num_erased3 = CleanupTrackers();
  ASSERT_EQ(num_erased3, 1);
  ASSERT_TRUE(tracker_gens_.empty());
  ASSERT_FALSE(tracker_gens_.Contains(1));
  ASSERT_FALSE(tracker_gens_.Contains(2));
  ASSERT_FALSE(tracker_gens_.Contains(3));
  ASSERT_NOT_OK(tracker_gens_.GetActive());
}

}  // namespace stirling
}  // namespace px
