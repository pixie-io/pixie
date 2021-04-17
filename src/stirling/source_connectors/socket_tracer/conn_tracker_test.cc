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

#include "src/stirling/source_connectors/socket_tracer/conn_tracker.h"

#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include <magic_enum.hpp>

#include "src/common/base/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"

namespace px {
namespace stirling {

// Automatically converts ToString() to stream operator for gtest.
using ::px::operator<<;

namespace http = protocols::http;
namespace mysql = protocols::mysql;

using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;

using testing::kHTTPReq0;
using testing::kHTTPReq1;
using testing::kHTTPReq2;
using testing::kHTTPResp0;
using testing::kHTTPResp1;
using testing::kHTTPResp2;
using testing::kHTTPUpgradeReq;
using testing::kHTTPUpgradeResp;

class ConnTrackerTest : public ::testing::Test {
 protected:
  ConnTrackerTest() : event_gen_(&real_clock_) {}

  testing::RealClock real_clock_;
  testing::EventGenerator event_gen_;
};

TEST_F(ConnTrackerTest, timestamp_test) {
  // Use mock clock to get precise timestamps.
  testing::MockClock mock_clock;
  testing::EventGenerator event_gen(&mock_clock);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> event0 = event_gen.InitSendEvent<kProtocolHTTP>("event0");
  std::unique_ptr<SocketDataEvent> event1 = event_gen.InitRecvEvent<kProtocolHTTP>("event1");
  std::unique_ptr<SocketDataEvent> event2 = event_gen.InitSendEvent<kProtocolHTTP>("event2");
  std::unique_ptr<SocketDataEvent> event3 = event_gen.InitRecvEvent<kProtocolHTTP>("event3");
  std::unique_ptr<SocketDataEvent> event4 = event_gen.InitSendEvent<kProtocolHTTP>("event4");
  std::unique_ptr<SocketDataEvent> event5 = event_gen.InitRecvEvent<kProtocolHTTP>("event5");
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  EXPECT_EQ(0, tracker.last_bpf_timestamp_ns());
  tracker.AddControlEvent(conn);
  EXPECT_EQ(1, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(std::move(event0));
  EXPECT_EQ(2, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(std::move(event1));
  EXPECT_EQ(3, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(std::move(event5));
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(std::move(event2));
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(std::move(event3));
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(std::move(event4));
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddControlEvent(close_event);
  EXPECT_EQ(8, tracker.last_bpf_timestamp_ns());
}

TEST_F(ConnTrackerTest, ReqRespMatchingSimple) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(3, records.size());

  EXPECT_EQ(records[0].req.req_path, "/index.html");
  EXPECT_EQ(records[0].resp.body, "pixie");

  EXPECT_EQ(records[1].req.req_path, "/foo.html");
  EXPECT_EQ(records[1].resp.body, "foo");

  EXPECT_EQ(records[2].req.req_path, "/bar.html");
  EXPECT_EQ(records[2].resp.body, "bar");
}

TEST_F(ConnTrackerTest, DISABLED_ReqRespMatchingPipelined) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(3, records.size());

  EXPECT_EQ(records[0].req.req_path, "/index.html");
  EXPECT_EQ(records[0].resp.body, "pixie");

  EXPECT_EQ(records[1].req.req_path, "/foo.html");
  EXPECT_EQ(records[1].resp.body, "foo");

  EXPECT_EQ(records[2].req.req_path, "/bar.html");
  EXPECT_EQ(records[2].resp.body, "bar");
}

TEST_F(ConnTrackerTest, ReqRespMatchingSerializedMissingRequest) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  PL_UNUSED(req1);  // Missing event.
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(2, records.size());

  EXPECT_EQ(records[0].req.req_path, "/index.html");
  EXPECT_EQ(records[0].resp.body, "pixie");

  EXPECT_EQ(records[1].req.req_path, "/bar.html");
  EXPECT_EQ(records[1].resp.body, "bar");
}

TEST_F(ConnTrackerTest, ReqRespMatchingSerializedMissingResponse) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  PL_UNUSED(req2);  // Missing event.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(2, records.size());

  EXPECT_EQ(records[0].req.req_path, "/index.html");
  EXPECT_EQ(records[0].resp.body, "pixie");

  EXPECT_EQ(records[1].req.req_path, "/bar.html");
  EXPECT_EQ(records[1].resp.body, "bar");
}

TEST_F(ConnTrackerTest, TrackerDisable) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen.InitSendEvent<kProtocolHTTP>("hello");
  std::unique_ptr<SocketDataEvent> resp2 =
      event_gen.InitRecvEvent<kProtocolHTTP>("hello to you too");
  std::unique_ptr<SocketDataEvent> req3 = event_gen.InitSendEvent<kProtocolHTTP>("good-bye");
  std::unique_ptr<SocketDataEvent> resp3 =
      event_gen.InitRecvEvent<kProtocolHTTP>("good-bye to you too");
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  std::vector<http::Record> records;

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));

  records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(2, records.size());
  ASSERT_FALSE(tracker.IsZombie());

  // Say this connection is not interesting to follow anymore.
  tracker.Disable();

  // More events arrive.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));

  records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(0, records.size());
  ASSERT_FALSE(tracker.IsZombie());

  tracker.AddDataEvent(std::move(req3));
  tracker.AddDataEvent(std::move(resp3));
  tracker.AddControlEvent(close_event);

  records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(0, records.size());
  ASSERT_TRUE(tracker.IsZombie());
}

TEST_F(ConnTrackerTest, TrackerHTTP101Disable) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPUpgradeReq);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPUpgradeResp);
  std::unique_ptr<SocketDataEvent> req2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req3 = event_gen.InitSendEvent<kProtocolHTTP>("good-bye");
  std::unique_ptr<SocketDataEvent> resp3 =
      event_gen.InitRecvEvent<kProtocolHTTP>("good-bye to you too");
  struct socket_control_event_t close_event = event_gen.InitClose();

  ConnTracker tracker;
  std::vector<http::Record> records;

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));

  records = tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.IterationPostTick();

  ASSERT_EQ(2, records.size());
  ASSERT_FALSE(tracker.IsZombie());

  // More events arrive after the connection Upgrade.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));

  // Since we previously received connection Upgrade, this tracker should be disabled.
  // All future calls to ProcessToRecords() should produce no results.

  // This is a bad test beyond this point,
  // because a disabled tracker would never call ProcessToRecords again in Stirling.
  // Currently, this causes a warning to fire that states ProcessToRecords should not be
  // run on a stream at EOS.
  // However, the test still passes, so we'll leave the test for now.

  records = tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.IterationPostTick();

  ASSERT_EQ(0, records.size());
  ASSERT_FALSE(tracker.IsZombie());

  tracker.AddDataEvent(std::move(req3));
  tracker.AddDataEvent(std::move(resp3));
  tracker.AddControlEvent(close_event);

  // The tracker should, however, still process the close event.

  records = tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.IterationPostTick();

  ASSERT_EQ(0, records.size());
  ASSERT_TRUE(tracker.IsZombie());
}

TEST(StatsTest, Increment) {
  ConnTracker::Stats stats;

  EXPECT_EQ(0, stats.Get(ConnTracker::Stats::Key::kDataEventSent));

  stats.Increment(ConnTracker::Stats::Key::kDataEventSent);
  EXPECT_EQ(1, stats.Get(ConnTracker::Stats::Key::kDataEventSent));

  stats.Increment(ConnTracker::Stats::Key::kDataEventSent, 5);
  EXPECT_EQ(6, stats.Get(ConnTracker::Stats::Key::kDataEventSent));
}

TEST_F(ConnTrackerTest, DataEventsChangesCounter) {
  auto frame0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPResp0);

  ConnTracker tracker;

  EXPECT_EQ(0, tracker.stats().Get(ConnTracker::Stats::Key::kBytesRecv));
  EXPECT_EQ(0, tracker.stats().Get(ConnTracker::Stats::Key::kBytesSent));

  tracker.AddDataEvent(std::move(frame0));
  tracker.AddDataEvent(std::move(frame1));

  EXPECT_EQ(kHTTPReq0.size(), tracker.stats().Get(ConnTracker::Stats::Key::kBytesRecv));
  EXPECT_EQ(kHTTPResp0.size(), tracker.stats().Get(ConnTracker::Stats::Key::kBytesSent));
}

TEST_F(ConnTrackerTest, HTTPStuckEventsAreRemoved) {
  // Use incomplete data to make it stuck.
  testing::EventGenerator event_gen(&real_clock_);
  auto data0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, 10));
  auto data1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(10, 10));

  ConnTracker tracker;

  tracker.AddDataEvent(std::move(data0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());
  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());
  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());

  // The 4th time, the stuck condition is detected and all data is purged.
  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_TRUE(tracker.req_data()->Empty<http::Message>());

  // Now the stuck count is reset, so the event is kept.
  tracker.AddDataEvent(std::move(data1));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());
}

TEST_F(ConnTrackerTest, HTTPMessagesErasedAfterExpiration) {
  testing::EventGenerator event_gen(&real_clock_);
  auto frame0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  auto frame2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame3 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);

  ConnTracker tracker;

  FLAGS_messages_size_limit_bytes = 10000;
  FLAGS_messages_expiration_duration_secs = 10000;

  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_THAT(tracker.req_frames<http::Message>(), SizeIs(1));

  FLAGS_messages_expiration_duration_secs = 0;

  tracker.ProcessToRecords<http::ProtocolTraits>();
  EXPECT_THAT(tracker.req_frames<http::Message>(), IsEmpty());

  // TODO(yzhao): It's not possible to test the response messages, as they are immediately exported
  // without waiting for the requests.
}

TEST_F(ConnTrackerTest, MySQLMessagesErasedAfterExpiration) {
  testing::EventGenerator event_gen(&real_clock_);
  auto msg0 =
      event_gen.InitSendEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(0, "\x03SELECT"));

  ConnTracker tracker;

  FLAGS_messages_size_limit_bytes = 10000;
  FLAGS_messages_expiration_duration_secs = 10000;

  tracker.AddDataEvent(std::move(msg0));
  tracker.ProcessToRecords<mysql::ProtocolTraits>();
  EXPECT_THAT(tracker.req_frames<mysql::Packet>(), SizeIs(1));

  FLAGS_messages_expiration_duration_secs = 0;

  tracker.ProcessToRecords<mysql::ProtocolTraits>();
  EXPECT_THAT(tracker.req_frames<mysql::Packet>(), IsEmpty());
}

// Tests that tracker state is kDisabled if the remote address is in the cluster's CIDR range.
TEST_F(ConnTrackerTest, TrackerDisabledForIntraClusterRemoteEndpoint) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();

  // Set an address that falls in the intra-cluster address range.
  testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));
  std::vector cidrs = {cidr};

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
}

// Tests that ConnTrcker state is not updated when no cluster CIDR is specified.
TEST_F(ConnTrackerTest, TrackerCollectingForClientSideTracingWithNoCIDR) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(/*cluster_cidrs*/ {}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kCollecting, tracker.state());
}

// Tests that tracker state is kDisabled if the remote address is Unix domain socket.
TEST_F(ConnTrackerTest, TrackerDisabledForUnixDomainSocket) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  conn.open.addr.sin6_family = AF_UNIX;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));
  std::vector cidrs = {cidr};

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.IterationPreTick(cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
}

// Tests that tracker state is kDisabled if the remote address is kOther (non-IP).
TEST_F(ConnTrackerTest, TrackerDisabledForOtherSockAddrFamily) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  // Any non-IP family works for testing purposes.
  conn.open.addr.sin6_family = AF_NETLINK;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));
  std::vector cidrs = {cidr};

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.IterationPreTick(cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
}

// Tests that tracker is disabled after mapping the addresses from IPv4 to IPv6.
TEST_F(ConnTrackerTest, TrackerDisabledAfterMapping) {
  {
    testing::EventGenerator event_gen(&real_clock_);
    struct socket_control_event_t conn = event_gen.InitConn();
    testing::SetIPv6RemoteAddr(&conn, "::ffff:1.2.3.4");

    CIDRBlock cidr;
    ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

    ConnTracker tracker;
    tracker.AddControlEvent(conn);
    tracker.SetProtocol(kProtocolHTTP, "testing");
    tracker.SetRole(kRoleClient, "testing");
    tracker.IterationPreTick({cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
    EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state())
        << "Got: " << magic_enum::enum_name(tracker.state());
  }
  {
    testing::EventGenerator event_gen(&real_clock_);
    struct socket_control_event_t conn = event_gen.InitConn();
    testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

    CIDRBlock cidr;
    ASSERT_OK(ParseCIDRBlock("::ffff:1.2.3.4/120", &cidr));

    ConnTracker tracker;
    tracker.AddControlEvent(conn);
    tracker.SetProtocol(kProtocolHTTP, "testing");
    tracker.SetRole(kRoleClient, "testing");
    tracker.IterationPreTick({cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
    EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state())
        << "Got: " << magic_enum::enum_name(tracker.state());
  }
}

TEST_F(ConnTrackerTest, DisabledDueToParsingFailureRate) {
  using mysql::testutils::GenErr;
  using mysql::testutils::GenRawPacket;

  testing::EventGenerator event_gen(&real_clock_);
  auto req_frame0 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x44 0x44 is not a valid MySQL command"));
  auto resp_frame0 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame1 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x55 0x55 is not a valid MySQL command"));
  auto resp_frame1 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame2 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x66 0x66 is not a valid MySQL command"));
  auto resp_frame2 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame3 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x77 0x77 is not a valid MySQL command"));
  auto resp_frame3 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame4 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x88 0x88 is not a valid MySQL command"));
  auto resp_frame4 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame5 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x99 0x99 is not a valid MySQL command"));
  auto resp_frame5 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame6 = event_gen.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\xaa 0xaa is not a valid MySQL command"));
  auto resp_frame6 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));

  ConnTracker tracker;
  std::vector<mysql::Record> records;

  tracker.AddDataEvent(std::move(req_frame0));
  tracker.AddDataEvent(std::move(resp_frame0));

  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
  EXPECT_EQ(records.size(), 0);

  tracker.AddDataEvent(std::move(req_frame1));
  tracker.AddDataEvent(std::move(resp_frame1));
  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
  EXPECT_EQ(records.size(), 0);

  tracker.AddDataEvent(std::move(req_frame2));
  tracker.AddDataEvent(std::move(resp_frame2));
  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
  EXPECT_EQ(records.size(), 0);

  tracker.AddDataEvent(std::move(req_frame3));
  tracker.AddDataEvent(std::move(resp_frame3));
  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
  EXPECT_EQ(records.size(), 0);

  tracker.AddDataEvent(std::move(req_frame4));
  tracker.AddDataEvent(std::move(resp_frame4));
  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
  EXPECT_EQ(records.size(), 0);

  // This request should push the error rate above the brink.
  tracker.AddDataEvent(std::move(req_frame5));
  tracker.AddDataEvent(std::move(resp_frame5));
  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(records.size(), 0);
}

TEST_F(ConnTrackerTest, DisabledDueToStitchingFailureRate) {
  using mysql::testutils::GenErr;
  using mysql::testutils::GenRawPacket;

  testing::EventGenerator event_gen(&real_clock_);
  auto req_frame0 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 A"));
  auto resp_frame0 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame1 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 B"));
  auto resp_frame1 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame2 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 C"));
  auto resp_frame2 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame3 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 D"));
  auto resp_frame3 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame4 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 E"));
  auto resp_frame4 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame5 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 F"));
  auto resp_frame5 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame6 = event_gen.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 G"));
  auto resp_frame6 = event_gen.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));

  ConnTracker tracker;
  std::vector<mysql::Record> records;

  tracker.AddDataEvent(std::move(req_frame0));
  tracker.AddDataEvent(std::move(resp_frame0));
  tracker.AddDataEvent(std::move(req_frame1));
  tracker.AddDataEvent(std::move(resp_frame1));
  tracker.AddDataEvent(std::move(req_frame2));
  tracker.AddDataEvent(std::move(resp_frame2));
  tracker.AddDataEvent(std::move(req_frame3));
  tracker.AddDataEvent(std::move(resp_frame3));
  tracker.AddDataEvent(std::move(req_frame4));
  tracker.AddDataEvent(std::move(resp_frame4));

  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
  EXPECT_EQ(records.size(), 0);

  // This request should push the error rate above the brink.
  tracker.AddDataEvent(std::move(req_frame5));
  tracker.AddDataEvent(std::move(resp_frame5));
  records = tracker.ProcessToRecords<mysql::ProtocolTraits>();
  tracker.IterationPostTick();

  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(tracker.disable_reason(),
            "Connection does not appear to produce valid records of protocol kProtocolMySQL");
  EXPECT_EQ(records.size(), 0);
}

auto AggKeyIs(int tgid, std::string remote_addr) {
  return AllOf(Field(&ConnStats::AggKey::upid, Field(&upid_t::tgid, tgid)),
               Field(&ConnStats::AggKey::remote_addr, remote_addr));
}

auto StatsIs(int open, int close, int sent, int recv) {
  return AllOf(
      Field(&ConnStats::Stats::conn_open, open), Field(&ConnStats::Stats::conn_close, close),
      Field(&ConnStats::Stats::bytes_sent, sent), Field(&ConnStats::Stats::bytes_recv, recv));
}

// Test cases for protocols and roles are enumerated to avoid uncertainty in the handling in all of
// the combinations of protocols and roles; as the handling of protocols and roles is quite
// complicated.
class ConnTrackerStatsTest
    : public ConnTrackerTest,
      public ::testing::WithParamInterface<std::tuple<TrafficProtocol, EndpointRole>> {
 protected:
  void SetUp() override { tracker_.set_conn_stats(&conn_stats_); }

  ConnStats conn_stats_;
  ConnTracker tracker_;
};

// Tests that ConnTracker accepts conn_open, data, and conn_close events.
TEST_P(ConnTrackerStatsTest, ConnOpenDataCloseSequence) {
  TrafficProtocol protocol;
  EndpointRole role;

  std::tie(protocol, role) = GetParam();

  auto open_event = event_gen_.InitConn(role);
  auto req_frame0 = event_gen_.InitSendEvent(protocol, role, "aaaa");
  auto resp_frame0 = event_gen_.InitRecvEvent(protocol, role, "bbbb");
  auto close_event = event_gen_.InitClose();

  tracker_.AddControlEvent(open_event);

  // No stats pushed for conn_open.
  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 0, 0, 0))));

  tracker_.AddDataEvent(std::move(req_frame0));

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 0, 4, 0))));

  tracker_.AddDataEvent(std::move(resp_frame0));

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 0, 4, 4))));

  tracker_.AddControlEvent(close_event);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 1, 4, 4))));
}

// Tests that ConnTracker accepts data and conn_close events.
TEST_P(ConnTrackerStatsTest, NoConnOpen) {
  TrafficProtocol protocol;
  EndpointRole role;

  std::tie(protocol, role) = GetParam();

  auto open_event = event_gen_.InitConn();
  open_event.open.role = role;
  auto req_frame0 = event_gen_.InitSendEvent(protocol, role, "aaaa");
  auto resp_frame0 = event_gen_.InitRecvEvent(protocol, role, "bbbb");
  auto close_event = event_gen_.InitClose();

  // No stats pushed for conn_open.
  EXPECT_THAT(conn_stats_.mutable_agg_stats(), IsEmpty());

  tracker_.AddDataEvent(std::move(req_frame0));
  tracker_.AddDataEvent(std::move(resp_frame0));

  // Simulate a successful remote endpoint resolution.
  tracker_.AddControlEvent(open_event);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 0, 4, 4))));

  tracker_.AddControlEvent(close_event);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 1, 4, 4))));
}

// Tests that receiving conn_open and conn_close before any data event results into correct stats.
TEST_P(ConnTrackerStatsTest, OnlyDataEvents) {
  TrafficProtocol protocol;
  EndpointRole role;

  std::tie(protocol, role) = GetParam();

  auto open_event = event_gen_.InitConn();
  auto req_frame0 = event_gen_.InitSendEvent(protocol, role, "aaaa");
  auto resp_frame0 = event_gen_.InitRecvEvent(protocol, role, "bbbb");
  // No close_event.

  // No stats pushed for conn_open.
  EXPECT_THAT(conn_stats_.mutable_agg_stats(), IsEmpty());

  tracker_.AddDataEvent(std::move(req_frame0));
  tracker_.AddDataEvent(std::move(resp_frame0));

  // Simulate a successful remote endpoint resolution.
  tracker_.AddControlEvent(open_event);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 0, 4, 4))));

  // Trigger an inferred connection close.
  tracker_.MarkForDeath(0);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              UnorderedElementsAre(Pair(AggKeyIs(12345, "0.0.0.0"), StatsIs(1, 1, 4, 4))));
}

// Only instantiate tests for a partial list of protocols. As the code for computing the stats for
// different protocols are the same.
INSTANTIATE_TEST_SUITE_P(AllProtocols, ConnTrackerStatsTest,
                         ::testing::Combine(::testing::Values(kProtocolUnknown, kProtocolHTTP,
                                                              kProtocolMySQL, kProtocolCQL,
                                                              kProtocolPGSQL, kProtocolDNS),
                                            ::testing::Values(kRoleClient, kRoleServer)));

}  // namespace stirling
}  // namespace px
