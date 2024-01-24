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

#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"

#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"

namespace px {
namespace stirling {

// Automatically converts ToString() to stream operator for gtest.
using ::px::operator<<;

using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

TEST(HashTest, CanBeUsedInFlatHashMap) {
  absl::flat_hash_map<ConnStats::AggKey, int> map;
  EXPECT_THAT(map, IsEmpty());

  ConnStats::AggKey key = {
      .upid = {.tgid = 1, .start_time_ticks = 2},
      .remote_addr = "test",
      .remote_port = 12345,
  };

  map[key] = 1;
  EXPECT_THAT(map, SizeIs(1));
  map[key] = 2;
  EXPECT_THAT(map, SizeIs(1));

  ConnStats::AggKey key_diff_upid = key;
  key_diff_upid.upid = {.tgid = 1, .start_time_ticks = 3},

  map[key_diff_upid] = 1;
  EXPECT_THAT(map, SizeIs(2));
  map[key] = 2;
  EXPECT_THAT(map, SizeIs(2));
}

class ConnStatsTest : public ::testing::Test {
 protected:
  ConnStatsTest() : conn_stats_(&conn_trackers_mgr_) {}

  ConnTrackersManager conn_trackers_mgr_;
  ConnStats conn_stats_;
};

auto AggKeyIs(int tgid, std::string_view remote_addr, int remote_port) {
  return AllOf(Field(&ConnStats::AggKey::upid, Field(&upid_t::tgid, tgid)),
               Field(&ConnStats::AggKey::remote_addr, remote_addr),
               Field(&ConnStats::AggKey::remote_port, remote_port));
}

auto StatsIs(int open, int close, int sent, int recv) {
  return AllOf(
      Field(&ConnStats::Stats::conn_open, open), Field(&ConnStats::Stats::conn_close, close),
      Field(&ConnStats::Stats::bytes_sent, sent), Field(&ConnStats::Stats::bytes_recv, recv));
}

// Model the arrival of ConnStats events from BPF into the trackers,
// then check that the various events are aggregated properly by UpdateStats()
// to generate stats snapshots at different time intervals.
TEST_F(ConnStatsTest, Basic) {
  constexpr struct conn_id_t kConnID0 = {
      .upid = {.pid = 12345, .start_time_ticks = 1000},
      .fd = 3,
      .tsid = 111110,
  };

  // The basic conn_stats_event template.
  struct conn_stats_event_t conn_stats_event;
  conn_stats_event.timestamp_ns = 0;
  conn_stats_event.conn_id = kConnID0;
  conn_stats_event.role = kRoleClient;
  conn_stats_event.raddr.in4.sin_family = AF_INET;
  conn_stats_event.raddr.in4.sin_port = htons(80);
  conn_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn_stats_event.conn_events = 0;
  conn_stats_event.rd_bytes = 0;
  conn_stats_event.wr_bytes = 0;

  // This is the tracker to which we will be sending ConnStats events.
  ConnTracker& tracker = conn_trackers_mgr_.GetOrCreateConnTracker(conn_stats_event.conn_id);

  // Event: just a conn_open with no bytes transferred.
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.conn_events |= CONN_OPEN;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 80), StatsIs(1, 0, 0, 0))));

  // Event: some write traffic.
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.wr_bytes += 3;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 80), StatsIs(1, 0, 3, 0))));

  // Event: some mixed read/write traffic.
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.rd_bytes += 5;
  conn_stats_event.wr_bytes += 3;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 80), StatsIs(1, 0, 6, 5))));

  // Event: some mixed read/write traffic.
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.rd_bytes += 4;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 80), StatsIs(1, 0, 6, 9))));

  // Event: no new traffic.
  conn_stats_event.timestamp_ns += 1;

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 80), StatsIs(1, 0, 6, 9))));

  // Event: connection close.
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.conn_events |= CONN_CLOSE;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 80), StatsIs(1, 1, 6, 9))));
}

// Model various connections from clients to one server.
// Check that server stats have the right aggregations.
//   Client0 -> Server (1st connection)
//   Client0 -> Server (2nd connection)
//   Client1 -> Server (1st connection)
TEST_F(ConnStatsTest, ServerSide) {
  // First connection from Client 0.
  constexpr struct conn_id_t kConnID0 = {
      .upid = {.pid = 12345, .start_time_ticks = 1000},
      .fd = 3,
      .tsid = 10000,
  };

  struct conn_stats_event_t conn0_stats_event;
  conn0_stats_event.timestamp_ns = 0;
  conn0_stats_event.conn_id = kConnID0;
  conn0_stats_event.role = kRoleServer;
  conn0_stats_event.raddr.in4.sin_family = AF_INET;
  conn0_stats_event.raddr.in4.sin_port = 54321;
  conn0_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn0_stats_event.conn_events = 0;
  conn0_stats_event.rd_bytes = 0;
  conn0_stats_event.wr_bytes = 0;

  ConnTracker& tracker0 = conn_trackers_mgr_.GetOrCreateConnTracker(conn0_stats_event.conn_id);
  conn0_stats_event.timestamp_ns += 1;
  conn0_stats_event.conn_events |= CONN_OPEN | CONN_CLOSE;
  conn0_stats_event.rd_bytes = 100;
  conn0_stats_event.wr_bytes = 200;
  tracker0.AddConnStats(conn0_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 0), StatsIs(1, 1, 200, 100))));

  // Second connection from Client 0.
  constexpr struct conn_id_t kConnID1 = {
      .upid = {.pid = 12345, .start_time_ticks = 1000},
      .fd = 4,
      .tsid = 20000,
  };

  struct conn_stats_event_t conn1_stats_event;
  conn1_stats_event.timestamp_ns = 0;
  conn1_stats_event.conn_id = kConnID1;
  conn1_stats_event.role = kRoleServer;
  conn1_stats_event.raddr.in4.sin_family = AF_INET;
  conn1_stats_event.raddr.in4.sin_port = 65432;
  conn1_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn1_stats_event.conn_events = 0;
  conn1_stats_event.rd_bytes = 0;
  conn1_stats_event.wr_bytes = 0;

  ConnTracker& tracker1 = conn_trackers_mgr_.GetOrCreateConnTracker(conn1_stats_event.conn_id);
  conn1_stats_event.timestamp_ns += 1;
  conn1_stats_event.conn_events |= CONN_OPEN;
  conn1_stats_event.rd_bytes = 200;
  conn1_stats_event.wr_bytes = 400;
  tracker1.AddConnStats(conn1_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 0), StatsIs(2, 1, 600, 300))));

  // First connection from Client 1.
  constexpr struct conn_id_t kConnID3 = {
      .upid = {.pid = 12345, .start_time_ticks = 1000},
      .fd = 5,
      .tsid = 30000,
  };

  struct conn_stats_event_t conn2_stats_event;
  conn2_stats_event.timestamp_ns = 0;
  conn2_stats_event.conn_id = kConnID3;
  conn2_stats_event.role = kRoleServer;
  conn2_stats_event.raddr.in4.sin_family = AF_INET;
  conn2_stats_event.raddr.in4.sin_port = 12345;
  conn2_stats_event.raddr.in4.sin_addr.s_addr = 0x02020202;  // 2.2.2.2
  conn2_stats_event.conn_events = 0;
  conn2_stats_event.rd_bytes = 0;
  conn2_stats_event.wr_bytes = 0;

  ConnTracker& tracker2 = conn_trackers_mgr_.GetOrCreateConnTracker(conn2_stats_event.conn_id);
  conn2_stats_event.timestamp_ns += 1;
  conn2_stats_event.conn_events |= CONN_OPEN;
  conn2_stats_event.rd_bytes = 400;
  conn2_stats_event.wr_bytes = 800;
  tracker2.AddConnStats(conn2_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "1.1.1.1", 0), StatsIs(2, 1, 600, 300)),
                                   Pair(AggKeyIs(12345, "2.2.2.2", 0), StatsIs(1, 0, 800, 400))));
}

// Model various connections from one client to various servers.
// Check that client stats have the right aggregations.
//   Client -> Server0 (1st connection)
//   Client -> Server0 (2nd connection)
//   Client -> Server1 (1st connection)
TEST_F(ConnStatsTest, ClientSide) {
  // First connection to Server 0.
  constexpr struct conn_id_t kConnID0 = {
      .upid = {.pid = 11111, .start_time_ticks = 1000},
      .fd = 3,
      .tsid = 10000,
  };

  struct conn_stats_event_t conn0_stats_event;
  conn0_stats_event.timestamp_ns = 0;
  conn0_stats_event.conn_id = kConnID0;
  conn0_stats_event.role = kRoleClient;
  conn0_stats_event.raddr.in4.sin_family = AF_INET;
  conn0_stats_event.raddr.in4.sin_port = htons(80);
  conn0_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn0_stats_event.conn_events = 0;
  conn0_stats_event.rd_bytes = 0;
  conn0_stats_event.wr_bytes = 0;

  ConnTracker& tracker0 = conn_trackers_mgr_.GetOrCreateConnTracker(conn0_stats_event.conn_id);
  conn0_stats_event.timestamp_ns += 1;
  conn0_stats_event.conn_events |= CONN_OPEN | CONN_CLOSE;
  conn0_stats_event.rd_bytes = 100;
  conn0_stats_event.wr_bytes = 200;
  tracker0.AddConnStats(conn0_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              ElementsAre(Pair(AggKeyIs(11111, "1.1.1.1", 80), StatsIs(1, 1, 200, 100))));

  // Second connection to Server 0.
  constexpr struct conn_id_t kConnID1 = {
      .upid = {.pid = 11111, .start_time_ticks = 1000},
      .fd = 4,
      .tsid = 20000,
  };

  struct conn_stats_event_t conn1_stats_event;
  conn1_stats_event.timestamp_ns = 0;
  conn1_stats_event.conn_id = kConnID1;
  conn1_stats_event.role = kRoleClient;
  conn1_stats_event.raddr.in4.sin_family = AF_INET;
  conn1_stats_event.raddr.in4.sin_port = htons(80);
  conn1_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn1_stats_event.conn_events = 0;
  conn1_stats_event.rd_bytes = 0;
  conn1_stats_event.wr_bytes = 0;

  ConnTracker& tracker1 = conn_trackers_mgr_.GetOrCreateConnTracker(conn1_stats_event.conn_id);
  conn1_stats_event.timestamp_ns += 1;
  conn1_stats_event.conn_events |= CONN_OPEN;
  conn1_stats_event.rd_bytes = 200;
  conn1_stats_event.wr_bytes = 400;
  tracker1.AddConnStats(conn1_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              ElementsAre(Pair(AggKeyIs(11111, "1.1.1.1", 80), StatsIs(2, 1, 600, 300))));

  // First connection to Server 1.
  constexpr struct conn_id_t kConnID3 = {
      .upid = {.pid = 11111, .start_time_ticks = 1000},
      .fd = 5,
      .tsid = 30000,
  };

  struct conn_stats_event_t conn2_stats_event;
  conn2_stats_event.timestamp_ns = 0;
  conn2_stats_event.conn_id = kConnID3;
  conn2_stats_event.role = kRoleClient;
  conn2_stats_event.raddr.in4.sin_family = AF_INET;
  conn2_stats_event.raddr.in4.sin_port = htons(21);
  conn2_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn2_stats_event.conn_events = 0;
  conn2_stats_event.rd_bytes = 0;
  conn2_stats_event.wr_bytes = 0;

  ConnTracker& tracker2 = conn_trackers_mgr_.GetOrCreateConnTracker(conn2_stats_event.conn_id);
  conn2_stats_event.timestamp_ns += 1;
  conn2_stats_event.conn_events |= CONN_OPEN;
  conn2_stats_event.rd_bytes = 400;
  conn2_stats_event.wr_bytes = 800;
  tracker2.AddConnStats(conn2_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(),
              UnorderedElementsAre(Pair(AggKeyIs(11111, "1.1.1.1", 80), StatsIs(2, 1, 600, 300)),
                                   Pair(AggKeyIs(11111, "1.1.1.1", 21), StatsIs(1, 0, 800, 400))));
}

// Tests that any connection trackers with no remote endpoint do not report conn stats events.
TEST_F(ConnStatsTest, NoEventsIfNoRemoteAddr) {
  constexpr struct conn_id_t kConnID0 = {
      .upid = {.pid = 11111, .start_time_ticks = 1000},
      .fd = 3,
      .tsid = 10000,
  };

  struct conn_stats_event_t conn_stats_event;
  conn_stats_event.timestamp_ns = 0;
  conn_stats_event.conn_id = kConnID0;
  conn_stats_event.role = kRoleClient;
  conn_stats_event.raddr.in4.sin_family = PX_AF_UNKNOWN;
  conn_stats_event.conn_events = 0;
  conn_stats_event.rd_bytes = 0;
  conn_stats_event.wr_bytes = 0;

  ConnTracker& tracker = conn_trackers_mgr_.GetOrCreateConnTracker(conn_stats_event.conn_id);
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.conn_events |= CONN_OPEN | CONN_CLOSE;
  conn_stats_event.rd_bytes = 100;
  conn_stats_event.wr_bytes = 200;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_THAT(conn_stats_.UpdateStats(), IsEmpty());
}

// Tests that disabled ConnTracker still reports data.
TEST_F(ConnStatsTest, DisabledConnTracker) {
  constexpr struct conn_id_t kConnID0 = {
      .upid = {.pid = 11111, .start_time_ticks = 1000},
      .fd = 3,
      .tsid = 10000,
  };

  struct conn_stats_event_t conn_stats_event;
  conn_stats_event.timestamp_ns = 0;
  conn_stats_event.conn_id = kConnID0;
  conn_stats_event.role = kRoleClient;
  conn_stats_event.raddr.in4.sin_family = AF_INET;
  conn_stats_event.raddr.in4.sin_port = htons(80);
  conn_stats_event.raddr.in4.sin_addr.s_addr = 0x01010101;  // 1.1.1.1
  conn_stats_event.conn_events = 0;
  conn_stats_event.rd_bytes = 0;
  conn_stats_event.wr_bytes = 0;

  ConnTracker& tracker = conn_trackers_mgr_.GetOrCreateConnTracker(conn_stats_event.conn_id);
  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.conn_events |= CONN_OPEN | CONN_CLOSE;
  conn_stats_event.rd_bytes = 100;
  conn_stats_event.wr_bytes = 200;
  tracker.AddConnStats(conn_stats_event);
  tracker.Disable("Just because");

  EXPECT_THAT(conn_stats_.UpdateStats(),
              ElementsAre(Pair(AggKeyIs(11111, "1.1.1.1", 80), StatsIs(1, 1, 200, 100))));
}

}  // namespace stirling
}  // namespace px
