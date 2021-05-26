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
  ConnTrackerTest() : event_gen_(&real_clock_), now_(std::chrono::steady_clock::now()) {}

  testing::RealClock real_clock_;
  testing::EventGenerator event_gen_;

  std::chrono::time_point<std::chrono::steady_clock> now_;
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

TEST_F(ConnTrackerTest, DataEventsChangesCounter) {
  auto frame0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPResp0);

  ConnTracker tracker;

  EXPECT_EQ(0, tracker.GetStat(ConnTracker::StatKey::kBytesRecv));
  EXPECT_EQ(0, tracker.GetStat(ConnTracker::StatKey::kBytesSent));

  tracker.AddDataEvent(std::move(frame0));
  tracker.AddDataEvent(std::move(frame1));

  EXPECT_EQ(kHTTPReq0.size(), tracker.GetStat(ConnTracker::StatKey::kBytesRecv));
  EXPECT_EQ(kHTTPResp0.size(), tracker.GetStat(ConnTracker::StatKey::kBytesSent));
}

TEST_F(ConnTrackerTest, HTTPStuckEventsAreRemoved) {
  // Use incomplete data to make it stuck.
  testing::EventGenerator event_gen(&real_clock_);
  auto data0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, 10));
  auto data1 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(10, 10));

  ConnTracker tracker;

  // Set limit and expiry very large to make them non-factors.
  int size_limit_bytes = 10000;
  auto expiry_timestamp = std::chrono::steady_clock::now() - std::chrono::seconds(10000);

  tracker.AddDataEvent(std::move(data0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());

  // The 4th time, the stuck condition is detected and all data is purged.
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_TRUE(tracker.req_data()->Empty<http::Message>());

  // Now the stuck count is reset, so the event is kept.
  tracker.AddDataEvent(std::move(data1));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_FALSE(tracker.req_data()->Empty<http::Message>());
}

TEST_F(ConnTrackerTest, MessagesErasedAfterExpiration) {
  testing::EventGenerator event_gen(&real_clock_);
  auto frame0 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  auto frame2 = event_gen.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame3 = event_gen.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);

  ConnTracker tracker;

  int size_limit_bytes = 10000;
  auto expiry_timestamp = std::chrono::steady_clock::now() - std::chrono::seconds(10000);
  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_THAT(tracker.req_frames<http::Message>(), SizeIs(1));

  expiry_timestamp = std::chrono::steady_clock::now();
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(size_limit_bytes, expiry_timestamp);
  EXPECT_THAT(tracker.req_frames<http::Message>(), IsEmpty());

  // TODO(yzhao): It's not possible to test the response messages, as they are immediately exported
  // without waiting for the requests.
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
  tracker.IterationPreTick(now_, cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
}

// Tests that tracker state is kDisabled if the remote address is localhost.
TEST_F(ConnTrackerTest, TrackerDisabledForLocalhostRemoteEndpoint) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  conn.open.addr.in6.sin6_addr = IN6ADDR_LOOPBACK_INIT;
  conn.open.addr.in6.sin6_family = AF_INET6;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));
  std::vector cidrs = {cidr};

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(now_, cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
}

// Tests that ConnTracker state is not updated when no cluster CIDR is specified.
TEST_F(ConnTrackerTest, TrackerCollectingForClientSideTracingWithNoCIDR) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(now_, /*cluster_cidrs*/ {}, /*proc_parser*/ nullptr,
                           /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kCollecting, tracker.state());
}

// Tests that tracker state is kDisabled if the remote address is Unix domain socket.
TEST_F(ConnTrackerTest, TrackerDisabledForUnixDomainSocket) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  conn.open.addr.in6.sin6_family = AF_UNIX;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));
  std::vector cidrs = {cidr};

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.IterationPreTick(now_, cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
}

// Tests that tracker state is kDisabled if the remote address is kOther (non-IP).
TEST_F(ConnTrackerTest, TrackerDisabledForOtherSockAddrFamily) {
  testing::EventGenerator event_gen(&real_clock_);
  struct socket_control_event_t conn = event_gen.InitConn();
  // Any non-IP family works for testing purposes.
  conn.open.addr.in6.sin6_family = AF_NETLINK;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));
  std::vector cidrs = {cidr};

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.IterationPreTick(now_, cidrs, /*proc_parser*/ nullptr, /*connections*/ nullptr);
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
    tracker.IterationPreTick(now_, {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
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
    tracker.IterationPreTick(now_, {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
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

TEST_F(ConnTrackerTest, ConnStats) {
  ConnTracker tracker;

  constexpr struct conn_id_t kConnID0 = {
      .upid = {.pid = 12345, .start_time_ticks = 1000},
      .fd = 3,
      .tsid = 111110,
  };

  struct conn_stats_event_t conn_stats_event;
  conn_stats_event.timestamp_ns = 0;
  conn_stats_event.conn_id = kConnID0;
  conn_stats_event.role = kRoleClient;
  reinterpret_cast<struct sockaddr_in*>(&conn_stats_event.addr)->sin_family = AF_INET;
  reinterpret_cast<struct sockaddr_in*>(&conn_stats_event.addr)->sin_port = htons(80);
  reinterpret_cast<struct sockaddr_in*>(&conn_stats_event.addr)->sin_addr.s_addr =
      0x01010101;  // 1.1.1.1
  conn_stats_event.conn_events = 0;
  conn_stats_event.rd_bytes = 0;
  conn_stats_event.wr_bytes = 0;

  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.conn_events |= CONN_OPEN;
  conn_stats_event.rd_bytes += 10;
  conn_stats_event.wr_bytes += 20;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_EQ(tracker.conn_stats().OpenSinceLastRead(), 1);
  EXPECT_EQ(tracker.conn_stats().CloseSinceLastRead(), 0);
  EXPECT_EQ(tracker.conn_stats().BytesRecvSinceLastRead(), 10);
  EXPECT_EQ(tracker.conn_stats().BytesSentSinceLastRead(), 20);

  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.rd_bytes += 10;
  conn_stats_event.wr_bytes += 20;
  tracker.AddConnStats(conn_stats_event);

  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.rd_bytes += 30;
  conn_stats_event.wr_bytes += 70;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_EQ(tracker.conn_stats().OpenSinceLastRead(), 0);
  EXPECT_EQ(tracker.conn_stats().CloseSinceLastRead(), 0);
  EXPECT_EQ(tracker.conn_stats().BytesRecvSinceLastRead(), 40);
  EXPECT_EQ(tracker.conn_stats().BytesSentSinceLastRead(), 90);

  conn_stats_event.timestamp_ns += 1;
  conn_stats_event.conn_events |= CONN_CLOSE;
  conn_stats_event.wr_bytes += 50;
  tracker.AddConnStats(conn_stats_event);

  EXPECT_EQ(tracker.conn_stats().OpenSinceLastRead(), 0);
  EXPECT_EQ(tracker.conn_stats().CloseSinceLastRead(), 1);
  EXPECT_EQ(tracker.conn_stats().BytesRecvSinceLastRead(), 0);
  EXPECT_EQ(tracker.conn_stats().BytesSentSinceLastRead(), 50);
}

}  // namespace stirling
}  // namespace px
