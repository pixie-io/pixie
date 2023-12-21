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

#include <chrono>
#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include <magic_enum.hpp>

#include "protocols/http/types.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"
#include "src/stirling/testing/common.h"

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
  ConnTrackerTest() : event_gen_(&mock_clock_) {}

  std::chrono::steady_clock::time_point now() {
    return testing::NanosToTimePoint(mock_clock_.now());
  }

  testing::MockClock mock_clock_;
  testing::EventGenerator event_gen_;
};

TEST_F(ConnTrackerTest, timestamp_test) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> event0 = event_gen_.InitSendEvent<kProtocolHTTP>("event0");
  std::unique_ptr<SocketDataEvent> event1 = event_gen_.InitRecvEvent<kProtocolHTTP>("event1");
  std::unique_ptr<SocketDataEvent> event2 = event_gen_.InitSendEvent<kProtocolHTTP>("event2");
  std::unique_ptr<SocketDataEvent> event3 = event_gen_.InitRecvEvent<kProtocolHTTP>("event3");
  std::unique_ptr<SocketDataEvent> event4 = event_gen_.InitSendEvent<kProtocolHTTP>("event4");
  std::unique_ptr<SocketDataEvent> event5 = event_gen_.InitRecvEvent<kProtocolHTTP>("event5");
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
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
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
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

TEST_F(ConnTrackerTest, ReqRespMatchingPipelinedIsNotSupported) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> records = tracker.ProcessToRecords<http::ProtocolTraits>();

  ASSERT_EQ(1, records.size());

  // This just shows that that the result is wrong when pipelining is active.
  EXPECT_EQ(records[0].req.req_path, "/bar.html");
  EXPECT_EQ(records[0].resp.body, "pixie");

  // A correct result would have been:
  //  EXPECT_EQ(records[0].req.req_path, "/bar.html");
  //  EXPECT_EQ(records[0].resp.body, "pixie");
  //
  //  EXPECT_EQ(records[1].req.req_path, "/foo.html");
  //  EXPECT_EQ(records[1].resp.body, "foo");
  //
  //  EXPECT_EQ(records[2].req.req_path, "/bar.html");
  //  EXPECT_EQ(records[2].resp.body, "bar");
}

TEST_F(ConnTrackerTest, ReqRespMatchingSerializedMissingRequest) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  PX_UNUSED(req1);  // Missing event.
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
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  PX_UNUSED(req2);  // Missing event.
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
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>("hello");
  std::unique_ptr<SocketDataEvent> resp2 =
      event_gen_.InitRecvEvent<kProtocolHTTP>("hello to you too");
  std::unique_ptr<SocketDataEvent> req3 = event_gen_.InitSendEvent<kProtocolHTTP>("good-bye");
  std::unique_ptr<SocketDataEvent> resp3 =
      event_gen_.InitRecvEvent<kProtocolHTTP>("good-bye to you too");
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
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
  struct socket_control_event_t conn = event_gen_.InitConn();
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPUpgradeReq);
  std::unique_ptr<SocketDataEvent> resp1 =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPUpgradeResp);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp2 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req3 = event_gen_.InitSendEvent<kProtocolHTTP>("good-bye");
  std::unique_ptr<SocketDataEvent> resp3 =
      event_gen_.InitRecvEvent<kProtocolHTTP>("good-bye to you too");
  struct socket_control_event_t close_event = event_gen_.InitClose();

  ConnTracker tracker;
  tracker.InitProtocolState<http::StateWrapper>();
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

TEST_F(ConnTrackerTest, MemUsage) {
  testing::MockClock mock_clock;
  testing::EventGenerator event_gen_(&mock_clock);
  struct socket_control_event_t conn = event_gen_.InitConn(kRoleServer);
  auto frame0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPResp0);

  ConnTracker tracker;
  tracker.InitFrames<http::stream_id_t, http::Message>();

  // Initial memory use is not 0, because the DataStreamBuffer has a small initial capacity.
  size_t mem_usage = tracker.MemUsage<http::ProtocolTraits>();
  EXPECT_GT(mem_usage, 0);
  EXPECT_LT(mem_usage, 50);

  // After adding events, the size should reflect that.
  tracker.AddControlEvent(std::move(conn));
  tracker.AddDataEvent(std::move(frame0));
  mem_usage = tracker.MemUsage<http::ProtocolTraits>();
  EXPECT_GE(mem_usage, kHTTPReq0.size());

  // ProcessToRecords should move the data to the parsed messages,
  // but since the response has not arrived yet, most of the memory should still be used.
  tracker.ProcessToRecords<http::ProtocolTraits>();
  mem_usage = tracker.MemUsage<http::ProtocolTraits>();
  EXPECT_GE(mem_usage, kHTTPReq0.size());

  // Second event should increase the size further.
  tracker.AddDataEvent(std::move(frame1));
  mem_usage = tracker.MemUsage<http::ProtocolTraits>();
  EXPECT_GE(mem_usage, kHTTPReq0.size() + kHTTPResp0.size());

  // This iteration of ProcessToRecords should output a record and the size should go back to zero.
  tracker.ProcessToRecords<http::ProtocolTraits>();
  // Set expiry_timestamp to 0 to prevent Cleanup from expiring data based on timestamp.
  std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp;
  tracker.Cleanup<http::ProtocolTraits>(1024 * 1024, 1024 * 1024, expiry_timestamp,
                                        expiry_timestamp);
  mem_usage = tracker.MemUsage<http::ProtocolTraits>();
  EXPECT_GT(mem_usage, 0);
  EXPECT_LT(mem_usage, 50);
}

TEST_F(ConnTrackerTest, BufferClearedAfterExpiration) {
  // Use incomplete data to make it stuck.
  auto data0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, 10));
  auto data1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);

  ConnTracker tracker;

  // Set limit and expiry very large to make them non-factors.
  int frame_size_limit_bytes = 10000;
  int buffer_size_limit_bytes = 10000;
  auto frame_expiry_timestamp = now() - std::chrono::seconds(10000);
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);

  tracker.AddDataEvent(std::move(data0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                        frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_FALSE(tracker.req_data()->data_buffer().empty());

  // After buffer expires upon timeout, all data in the buffer is purged.
  buffer_expiry_timestamp = now();
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                        frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_TRUE(tracker.req_data()->data_buffer().empty());

  // Set the buffer_expiry_timestamp to a long time ago, so next event is kept.
  buffer_expiry_timestamp = now() - std::chrono::seconds(10000);
  tracker.AddDataEvent(std::move(data1));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                        frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_EQ((tracker.req_data()->Frames<http::stream_id_t, http::Message>()[0].size()), 1);
}

TEST_F(ConnTrackerTest, BufferTruncatedBeyondSizeLimit) {
  auto data0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, 20));

  ConnTracker tracker;

  int frame_size_limit_bytes = 10000;
  int buffer_size_limit_bytes = 10;
  auto frame_expiry_timestamp = now() - std::chrono::seconds(10000);
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);

  tracker.AddDataEvent(std::move(data0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                        frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_EQ(tracker.req_data()->data_buffer().size(), buffer_size_limit_bytes);
  EXPECT_THAT((tracker.req_frames<http::stream_id_t, http::Message>()[0]), IsEmpty());
}

TEST_F(ConnTrackerTest, MessagesErasedAfterExpiration) {
  auto frame0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  auto frame2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame3 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);

  ConnTracker tracker;

  int frame_size_limit_bytes = 10000;
  int buffer_size_limit_bytes = 10000;
  auto frame_expiry_timestamp = now() - std::chrono::seconds(10000);
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);

  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                        frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_THAT((tracker.req_frames<http::stream_id_t, http::Message>()[0]), SizeIs(1));

  frame_expiry_timestamp = now();
  tracker.ProcessToRecords<http::ProtocolTraits>();
  tracker.Cleanup<http::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                        frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_THAT((tracker.req_frames<http::stream_id_t, http::Message>()[0]), IsEmpty());
}

// Tests that tracker state is kDisabled if the remote address is in the cluster's CIDR range.
TEST_F(ConnTrackerTest, DisabledForIntraClusterRemoteEndpoint) {
  struct socket_control_event_t conn = event_gen_.InitConn();

  // Set an address that falls in the intra-cluster address range.
  testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(now(), {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(std::string(tracker.disable_reason()),
            std::string("No client-side tracing: Remote endpoint is inside the cluster."));
}

// Tests that tracker state is kDisabled if the remote address is localhost.
TEST_F(ConnTrackerTest, DisabledForLocalhostRemoteEndpoint) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  conn.open.raddr.in6.sin6_addr = IN6ADDR_LOOPBACK_INIT;
  conn.open.raddr.in6.sin6_family = AF_INET6;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(now(), {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(std::string(tracker.disable_reason()),
            std::string("No client-side tracing: Remote endpoint is inside the cluster."));
}

// Tests that ConnTracker state is not updated when no cluster CIDR is specified.
TEST_F(ConnTrackerTest, TrackerCollectingForClientSideTracingWithNoCIDR) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(now(), /*cluster_cidrs*/ {}, /*proc_parser*/ nullptr,
                           /*connections*/ nullptr);
  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);
}

// Tests that tracker state is kDisabled if the remote address is Unix domain socket.
TEST_F(ConnTrackerTest, DisabledForUnixDomainSockets) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  conn.open.raddr.in6.sin6_family = AF_UNIX;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.IterationPreTick(now(), {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(std::string(tracker.disable_reason()), "Unhandled socket address family");
}

// Tests that tracker state is kDisabled if the remote address is kOther (non-IP).
TEST_F(ConnTrackerTest, DisabledForOtherSockAddrFamily) {
  struct socket_control_event_t conn = event_gen_.InitConn();
  // Any non-IP family works for testing purposes.
  conn.open.raddr.in6.sin6_family = AF_NETLINK;

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.IterationPreTick(now(), {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(std::string(tracker.disable_reason()), "Unhandled socket address family");
}

// Tests that tracker is disabled after mapping the addresses from IPv4 to IPv6.
// NOTE: This test has a lot of overlap with other tests. Consider removing it.
TEST_F(ConnTrackerTest, DisabledAfterMapping) {
  {
    struct socket_control_event_t conn = event_gen_.InitConn();
    testing::SetIPv6RemoteAddr(&conn, "::ffff:1.2.3.4");

    CIDRBlock cidr;
    ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

    ConnTracker tracker;
    tracker.AddControlEvent(conn);
    tracker.SetProtocol(kProtocolHTTP, "testing");
    tracker.SetRole(kRoleClient, "testing");
    tracker.IterationPreTick(now(), {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
    EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
    EXPECT_EQ(std::string(tracker.disable_reason()),
              std::string("No client-side tracing: Remote endpoint is inside the cluster."));
  }
  {
    struct socket_control_event_t conn = event_gen_.InitConn();
    testing::SetIPv4RemoteAddr(&conn, "1.2.3.4");

    CIDRBlock cidr;
    ASSERT_OK(ParseCIDRBlock("::ffff:1.2.3.4/120", &cidr));

    ConnTracker tracker;
    tracker.AddControlEvent(conn);
    tracker.SetProtocol(kProtocolHTTP, "testing");
    tracker.SetRole(kRoleClient, "testing");
    tracker.IterationPreTick(now(), {cidr}, /*proc_parser*/ nullptr, /*connections*/ nullptr);
    EXPECT_EQ(ConnTracker::State::kDisabled, tracker.state());
    EXPECT_EQ(std::string(tracker.disable_reason()),
              std::string("No client-side tracing: Remote endpoint is inside the cluster."));
  }
}

// Tests that tracker state is kDisabled if the remote address is kOther (non-IP).
TEST_F(ConnTrackerTest, DisabledForNonTrackedUPID) {
  struct socket_control_event_t conn = event_gen_.InitConn();

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  PX_SET_FOR_SCOPE(FLAGS_stirling_untracked_upid_threshold_seconds, 1);

  ConnTracker tracker;
  tracker.AddControlEvent(conn);
  tracker.SetProtocol(kProtocolHTTP, "testing");
  tracker.SetRole(kRoleClient, "testing");
  tracker.IterationPreTick(now() + std::chrono::seconds(30), {cidr}, /*proc_parser*/ nullptr,
                           /*connections*/ nullptr);
  EXPECT_EQ(tracker.state(), ConnTracker::State::kDisabled);
  EXPECT_EQ(std::string(tracker.disable_reason()), std::string("Not a tracked process."));
}

TEST_F(ConnTrackerTest, DisabledDueToParsingFailureRate) {
  using mysql::testutils::GenErr;
  using mysql::testutils::GenRawPacket;

  auto req_frame0 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x44 0x44 is not a valid MySQL command"));
  auto resp_frame0 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame1 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x55 0x55 is not a valid MySQL command"));
  auto resp_frame1 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame2 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x66 0x66 is not a valid MySQL command"));
  auto resp_frame2 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame3 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x77 0x77 is not a valid MySQL command"));
  auto resp_frame3 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame4 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x88 0x88 is not a valid MySQL command"));
  auto resp_frame4 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame5 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\x99 0x99 is not a valid MySQL command"));
  auto resp_frame5 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame6 = event_gen_.InitSendEvent<kProtocolMySQL>(
      GenRawPacket(0, "\xaa 0xaa is not a valid MySQL command"));
  auto resp_frame6 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));

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
  EXPECT_EQ(std::string(tracker.disable_reason()),
            std::string("Connection does not appear parseable as protocol kProtocolMySQL"));
  EXPECT_EQ(records.size(), 0);
}

TEST_F(ConnTrackerTest, DisabledDueToStitchingFailureRate) {
  using mysql::testutils::GenErr;
  using mysql::testutils::GenRawPacket;

  auto req_frame0 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 A"));
  auto resp_frame0 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame1 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 B"));
  auto resp_frame1 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame2 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 C"));
  auto resp_frame2 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame3 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 D"));
  auto resp_frame3 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame4 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 E"));
  auto resp_frame4 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame5 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 F"));
  auto resp_frame5 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));
  auto req_frame6 = event_gen_.InitSendEvent<kProtocolMySQL>(GenRawPacket(0, "\x03 G"));
  auto resp_frame6 = event_gen_.InitRecvEvent<kProtocolMySQL>(GenRawPacket(1, ""));

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
  EXPECT_EQ(std::string(tracker.disable_reason()),
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
  reinterpret_cast<struct sockaddr_in*>(&conn_stats_event.raddr)->sin_family = AF_INET;
  reinterpret_cast<struct sockaddr_in*>(&conn_stats_event.raddr)->sin_port = htons(80);
  reinterpret_cast<struct sockaddr_in*>(&conn_stats_event.raddr)->sin_addr.s_addr =
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

  for (auto p : magic_enum::enum_values<traffic_protocol_t>()) {
    for (auto r : {kRoleClient, kRoleServer}) {
      std::cout << absl::Substitute("UpdateStateParam{$0, $1, ConnTracker::State::kCollecting},\n",
                                    magic_enum::enum_name(p), magic_enum::enum_name(r));
    }
  }
}

struct UpdateStateParam {
  traffic_protocol_t protocol;
  endpoint_role_t role;
  ConnTracker::State expected_state;
};

class ConnTrackerTestDouble : public ConnTracker {
 public:
  using ConnTracker::UpdateState;
};

class ConnTrackerUpdateStateTest : public ::testing::TestWithParam<UpdateStateParam> {};

// Tests that ConnTracker::UpdateState() changes the state correctly.
TEST_P(ConnTrackerUpdateStateTest, UpdateStateBecauseOfRole) {
  ConnTrackerTestDouble tracker;
  EXPECT_EQ(tracker.state(), ConnTracker::State::kCollecting);

  EXPECT_TRUE(tracker.SetRole(GetParam().role, "test"));

  EXPECT_TRUE(tracker.SetProtocol(GetParam().protocol, "test"));

  tracker.UpdateState(/*cluster_cidrs*/ {});
  EXPECT_EQ(tracker.state(), GetParam().expected_state);
}

INSTANTIATE_TEST_SUITE_P(
    AllProtocols, ConnTrackerUpdateStateTest,
    ::testing::Values(
        UpdateStateParam{kProtocolHTTP, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolHTTP, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolHTTP2, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolHTTP2, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolMySQL, kRoleClient, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolMySQL, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolCQL, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolCQL, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolPGSQL, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolPGSQL, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolDNS, kRoleClient, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolDNS, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolRedis, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolRedis, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolNATS, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolNATS, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolMongo, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolMongo, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolKafka, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolKafka, kRoleServer, ConnTracker::State::kTransferring},
        UpdateStateParam{kProtocolMux, kRoleClient, ConnTracker::State::kCollecting},
        UpdateStateParam{kProtocolMux, kRoleServer, ConnTracker::State::kTransferring}));

}  // namespace stirling
}  // namespace px
