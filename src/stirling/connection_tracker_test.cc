#include "src/stirling/connection_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include "src/common/base/test_utils.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ::testing::IsEmpty;
using ::testing::SizeIs;

using ConnectionTrackerTest = testing::EventsFixture;

TEST_F(ConnectionTrackerTest, timestamp_test) {
  ConnectionTracker tracker;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> event0 = InitSendEvent<kProtocolHTTP>("event0");
  std::unique_ptr<SocketDataEvent> event1 = InitRecvEvent<kProtocolHTTP>("event1");
  std::unique_ptr<SocketDataEvent> event2 = InitSendEvent<kProtocolHTTP>("event2");
  std::unique_ptr<SocketDataEvent> event3 = InitRecvEvent<kProtocolHTTP>("event3");
  std::unique_ptr<SocketDataEvent> event4 = InitSendEvent<kProtocolHTTP>("event4");
  std::unique_ptr<SocketDataEvent> event5 = InitRecvEvent<kProtocolHTTP>("event5");
  struct socket_control_event_t close_event = InitClose();

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

// This test is of marginal value. Remove if it becomes hard to maintain.
TEST_F(ConnectionTrackerTest, info_string) {
  ConnectionTracker tracker;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> event0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> event1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> event2 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(event0));
  tracker.AddDataEvent(std::move(event1));
  tracker.AddDataEvent(std::move(event2));

  std::string debug_info = tracker.DebugString<http::Record>();

  std::string expected_output = R"(pid=12345 fd=3 gen=1
state=kCollecting
remote_addr=0.0.0.0:0
protocol=kProtocolHTTP
recv queue
  raw events=1
  parsed messages=0
send queue
  raw events=2
  parsed messages=0
)";

  EXPECT_EQ(expected_output, debug_info);

  tracker.ProcessMessages<http::Record>();

  debug_info = tracker.DebugString<http::Record>();

  expected_output = R"(pid=12345 fd=3 gen=1
state=kCollecting
remote_addr=0.0.0.0:0
protocol=kProtocolHTTP
recv queue
  raw events=0
  parsed messages=0
send queue
  raw events=0
  parsed messages=1
)";

  EXPECT_EQ(expected_output, debug_info);
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingSimple) {
  ConnectionTracker tracker;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = InitClose();

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> req_resp_pairs;
  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(3, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp.http_msg_body, "pixie");

  EXPECT_EQ(req_resp_pairs[1].req.http_req_path, "/foo.html");
  EXPECT_EQ(req_resp_pairs[1].resp.http_msg_body, "foo");

  EXPECT_EQ(req_resp_pairs[2].req.http_req_path, "/bar.html");
  EXPECT_EQ(req_resp_pairs[2].resp.http_msg_body, "bar");
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingPipelined) {
  ConnectionTracker tracker;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = InitClose();

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> req_resp_pairs;
  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(3, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp.http_msg_body, "pixie");

  EXPECT_EQ(req_resp_pairs[1].req.http_req_path, "/foo.html");
  EXPECT_EQ(req_resp_pairs[1].resp.http_msg_body, "foo");

  EXPECT_EQ(req_resp_pairs[2].req.http_req_path, "/bar.html");
  EXPECT_EQ(req_resp_pairs[2].resp.http_msg_body, "bar");
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingSerializedMissingRequest) {
  ConnectionTracker tracker;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = InitClose();

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  PL_UNUSED(req1);  // Missing event.
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> req_resp_pairs;
  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(3, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp.http_msg_body, "pixie");

  EXPECT_EQ(req_resp_pairs[1].req.http_req_path, "-");
  EXPECT_EQ(req_resp_pairs[1].resp.http_msg_body, "foo");

  EXPECT_EQ(req_resp_pairs[2].req.http_req_path, "/bar.html");
  EXPECT_EQ(req_resp_pairs[2].resp.http_msg_body, "bar");
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingSerializedMissingResponse) {
  ConnectionTracker tracker;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent<kProtocolHTTP>(kHTTPResp2);
  struct socket_control_event_t close_event = InitClose();

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  PL_UNUSED(req2);  // Missing event.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddControlEvent(close_event);

  std::vector<http::Record> req_resp_pairs;

  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(2, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp.http_msg_body, "pixie");

  // Oops - expecting a mismatch? Yes! What else can we do?
  EXPECT_EQ(req_resp_pairs[1].req.http_req_path, "/foo.html");
  EXPECT_EQ(req_resp_pairs[1].resp.http_msg_body, "bar");

  // Final request sticks around waiting for a partner - who will never come!
  // TODO(oazizi): The close should be an indicator that the partner will never come.
}

TEST_F(ConnectionTrackerTest, TrackerDisable) {
  ConnectionTracker tracker;
  std::vector<http::Record> req_resp_pairs;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>("hello");
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent<kProtocolHTTP>("hello to you too");
  std::unique_ptr<SocketDataEvent> req3 = InitSendEvent<kProtocolHTTP>("good-bye");
  std::unique_ptr<SocketDataEvent> resp3 = InitRecvEvent<kProtocolHTTP>("good-bye to you too");
  struct socket_control_event_t close_event = InitClose();

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));

  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(2, req_resp_pairs.size());
  ASSERT_FALSE(tracker.IsZombie());

  // Say this connection is not interesting to follow anymore.
  tracker.Disable();

  // More events arrive.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));

  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(0, req_resp_pairs.size());
  ASSERT_FALSE(tracker.IsZombie());

  tracker.AddDataEvent(std::move(req3));
  tracker.AddDataEvent(std::move(resp3));
  tracker.AddControlEvent(close_event);

  req_resp_pairs = tracker.ProcessMessages<http::Record>();

  ASSERT_EQ(0, req_resp_pairs.size());
  ASSERT_TRUE(tracker.IsZombie());
}

TEST_F(ConnectionTrackerTest, TrackerHTTP101Disable) {
  ConnectionTracker tracker;
  std::vector<http::Record> req_resp_pairs;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPUpgradeReq);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent<kProtocolHTTP>(kHTTPUpgradeResp);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req3 = InitSendEvent<kProtocolHTTP>("good-bye");
  std::unique_ptr<SocketDataEvent> resp3 = InitRecvEvent<kProtocolHTTP>("good-bye to you too");
  struct socket_control_event_t close_event = InitClose();

  tracker.AddControlEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));

  req_resp_pairs = tracker.ProcessMessages<http::Record>();
  tracker.IterationPostTick();

  ASSERT_EQ(2, req_resp_pairs.size());
  ASSERT_FALSE(tracker.IsZombie());

  // More events arrive after the connection Upgrade.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));

  // Since we previously received connection Upgrade, this tracker should be disabled.
  // All future calls to ProcessMessages() should produce no results.

  req_resp_pairs = tracker.ProcessMessages<http::Record>();
  tracker.IterationPostTick();

  ASSERT_EQ(0, req_resp_pairs.size());
  ASSERT_FALSE(tracker.IsZombie());

  tracker.AddDataEvent(std::move(req3));
  tracker.AddDataEvent(std::move(resp3));
  tracker.AddControlEvent(close_event);

  // The tracker should, however, still process the close event.

  req_resp_pairs = tracker.ProcessMessages<http::Record>();
  tracker.IterationPostTick();

  ASSERT_EQ(0, req_resp_pairs.size());
  ASSERT_TRUE(tracker.IsZombie());
}

TEST_F(ConnectionTrackerTest, stats_counter) {
  ConnectionTracker tracker;

  EXPECT_EQ(0, tracker.Stat(ConnectionTracker::CountStats::kDataEvent));

  tracker.IncrementStat(ConnectionTracker::CountStats::kDataEvent);
  EXPECT_EQ(1, tracker.Stat(ConnectionTracker::CountStats::kDataEvent));

  tracker.IncrementStat(ConnectionTracker::CountStats::kDataEvent);
  EXPECT_EQ(2, tracker.Stat(ConnectionTracker::CountStats::kDataEvent));
}

TEST_F(ConnectionTrackerTest, HTTP2ResetAfterStitchFailure) {
  ConnectionTracker tracker;

  auto frame0 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);
  auto frame1 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);

  auto frame2 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);
  auto frame3 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);

  auto frame4 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);
  auto frame5 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);

  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.resp_messages<http2::Frame>(), SizeIs(1));

  tracker.AddDataEvent(std::move(frame1));
  tracker.ProcessMessages<http2::Record>();
  // Now we see two END_STREAM headers frame on stream ID 1, then that translate to 2 gRPC
  // response messages. That failure will cause stream being reset.
  EXPECT_THAT(tracker.resp_messages<http2::Frame>(), IsEmpty());

  tracker.AddDataEvent(std::move(frame2));
  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.req_messages<http2::Frame>(), SizeIs(1));

  tracker.AddDataEvent(std::move(frame3));
  tracker.ProcessMessages<http2::Record>();
  // Ditto.
  EXPECT_THAT(tracker.req_messages<http2::Frame>(), IsEmpty());

  // Add a call to make sure things do not go haywire after resetting stream.
  tracker.AddDataEvent(std::move(frame4));
  tracker.AddDataEvent(std::move(frame5));
  auto req_resp_pairs = tracker.ProcessMessages<http2::Record>();
  // These 2 messages forms a matching req & resp.
  EXPECT_THAT(req_resp_pairs, SizeIs(1));
}

// TODO(yzhao): Add the same test for HTTPMessage.
TEST_F(ConnectionTrackerTest, HTTP2FramesCleanedUpAfterBreachingSizeLimit) {
  FLAGS_messages_size_limit_bytes = 10000;
  ConnectionTracker tracker;

  auto frame0 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);
  auto frame1 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);

  auto frame2 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);
  auto frame3 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);

  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.resp_messages<http2::Frame>(), SizeIs(1));

  // Set to 0 so it can expire immediately.
  FLAGS_messages_size_limit_bytes = 0;

  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.resp_messages<http2::Frame>(), IsEmpty());

  FLAGS_messages_size_limit_bytes = 10000;
  tracker.AddDataEvent(std::move(frame1));
  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.req_messages<http2::Frame>(), SizeIs(1));

  FLAGS_messages_size_limit_bytes = 0;
  tracker.ProcessMessages<http2::Record>();
  // Ditto.
  EXPECT_THAT(tracker.req_messages<http2::Frame>(), IsEmpty());

  // Add a call to make sure things do not go haywire after resetting stream.
  tracker.AddDataEvent(std::move(frame2));
  tracker.AddDataEvent(std::move(frame3));
  auto req_resp_pairs = tracker.ProcessMessages<http2::Record>();
  // These 2 messages forms a matching req & resp.
  EXPECT_THAT(req_resp_pairs, SizeIs(1));
}

TEST_F(ConnectionTrackerTest, HTTP2FramesErasedAfterExpiration) {
  FLAGS_messages_size_limit_bytes = 10000;
  ConnectionTracker tracker;

  auto frame0 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);
  auto frame1 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);

  auto frame2 = InitRecvEvent<kProtocolHTTP2>(kHTTP2EndStreamHeadersFrame);
  auto frame3 = InitSendEvent<kProtocolHTTP2>(kHTTP2EndStreamDataFrame);

  FLAGS_messages_expiration_duration_secs = 10000;

  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.resp_messages<http2::Frame>(), SizeIs(1));

  // Set to 0 so it can expire immediately.
  FLAGS_messages_expiration_duration_secs = 0;

  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.resp_messages<http2::Frame>(), IsEmpty());

  FLAGS_messages_expiration_duration_secs = 10000;
  tracker.AddDataEvent(std::move(frame1));
  tracker.ProcessMessages<http2::Record>();
  EXPECT_THAT(tracker.req_messages<http2::Frame>(), SizeIs(1));

  FLAGS_messages_expiration_duration_secs = 0;
  tracker.ProcessMessages<http2::Record>();
  // Ditto.
  EXPECT_THAT(tracker.req_messages<http2::Frame>(), IsEmpty());

  // Add a call to make sure things do not go haywire after resetting stream.
  tracker.AddDataEvent(std::move(frame2));
  tracker.AddDataEvent(std::move(frame3));
  auto req_resp_pairs = tracker.ProcessMessages<http2::Record>();
  // These 2 messages forms a matching req & resp.
  EXPECT_THAT(req_resp_pairs, SizeIs(1));
}

TEST_F(ConnectionTrackerTest, HTTPStuckEventsAreRemoved) {
  // Intentionally use non-HTTP data so make it stuck.
  auto frame0 = InitSendEvent<kProtocolHTTP>("aaaaaaaaa");
  auto frame1 = InitSendEvent<kProtocolHTTP>("aaaaaaaaa");
  auto frame2 = InitRecvEvent<kProtocolHTTP>("bbbbbbbbb");
  auto frame3 = InitRecvEvent<kProtocolHTTP>("bbbbbbbbb");

  ConnectionTracker tracker;
  {
    tracker.AddDataEvent(std::move(frame0));
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.req_data()->Empty<http::HTTPMessage>());
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.req_data()->Empty<http::HTTPMessage>());
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.req_data()->Empty<http::HTTPMessage>());

    // The 4th time, the stuck is detected and data are purged.
    tracker.ProcessMessages<http::Record>();
    EXPECT_TRUE(tracker.req_data()->Empty<http::HTTPMessage>());

    // Now the stuck count is reset, so the event is kept.
    tracker.AddDataEvent(std::move(frame1));
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.req_data()->Empty<http::HTTPMessage>());
  }
  {
    tracker.AddDataEvent(std::move(frame2));
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.resp_data()->Empty<http::HTTPMessage>());
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.resp_data()->Empty<http::HTTPMessage>());
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.resp_data()->Empty<http::HTTPMessage>());

    tracker.ProcessMessages<http::Record>();
    EXPECT_TRUE(tracker.resp_data()->Empty<http::HTTPMessage>());

    tracker.AddDataEvent(std::move(frame3));
    tracker.ProcessMessages<http::Record>();
    EXPECT_FALSE(tracker.resp_data()->Empty<http::HTTPMessage>());
  }
}

TEST_F(ConnectionTrackerTest, HTTPMessagesErasedAfterExpiration) {
  FLAGS_messages_size_limit_bytes = 10000;

  auto frame0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame1 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  auto frame2 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  auto frame3 = InitRecvEvent<kProtocolHTTP>(kHTTPResp0);

  ConnectionTracker tracker;

  FLAGS_messages_expiration_duration_secs = 10000;

  tracker.AddDataEvent(std::move(frame0));
  tracker.ProcessMessages<http::Record>();
  EXPECT_THAT(tracker.req_messages<http::HTTPMessage>(), SizeIs(1));

  FLAGS_messages_expiration_duration_secs = 0;

  tracker.ProcessMessages<http::Record>();
  EXPECT_THAT(tracker.req_messages<http::HTTPMessage>(), IsEmpty());

  // TODO(yzhao): It's not possible to test the response messages, as they are immediately exported
  // without waiting for the requests.
}

TEST_F(ConnectionTrackerTest, MySQLMessagesErasedAfterExpiration) {
  FLAGS_messages_size_limit_bytes = 10000;

  auto msg0 = InitSendEvent<kProtocolMySQL>(mysql::testutils::GenRawPacket(0, "\x03SELECT"));

  ConnectionTracker tracker;

  FLAGS_messages_expiration_duration_secs = 10000;

  tracker.AddDataEvent(std::move(msg0));
  tracker.ProcessMessages<mysql::Record>();
  EXPECT_THAT(tracker.req_messages<mysql::Packet>(), SizeIs(1));

  FLAGS_messages_expiration_duration_secs = 0;

  tracker.ProcessMessages<mysql::Record>();
  EXPECT_THAT(tracker.req_messages<mysql::Packet>(), IsEmpty());
}

// Tests that tracker state is kDisabled if the remote address is in the cluster's CIDR range.
TEST_F(ConnectionTrackerTest, TrackerDisabledForIntraClusterRemoteEndpoint) {
  ConnectionTracker tracker;
  std::vector<http::Record> req_resp_pairs;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  conn.open.traffic_class.role = EndpointRole::kRoleClient;

  // Set an address that falls in the intra-cluster address range.
  struct sockaddr_in v4_addr = {};
  v4_addr.sin_family = AF_INET;
  uint16_t port = 123;
  v4_addr.sin_port = htons(port);
  PL_CHECK_OK(ParseIPv4Addr("1.2.3.4", &v4_addr.sin_addr));
  memcpy(&conn.open.addr, &v4_addr, sizeof(struct sockaddr_in));

  tracker.AddControlEvent(conn);

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  tracker.IterationPreTick(cidr, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnectionTracker::State::kDisabled, tracker.state());
}

// TODO(oazizi): Re-enabled this test once the SockAddr work is complete.
// Tests that tracker state is kDisabled if the remote address is Unix domain socket.
TEST_F(ConnectionTrackerTest, DISABLED_TrackerDisabledForUnixDomainSocket) {
  ConnectionTracker tracker;
  std::vector<http::Record> req_resp_pairs;

  struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
  conn.open.traffic_class.role = EndpointRole::kRoleServer;

  // Set an address that falls in the intra-cluster address range.
  struct sockaddr_in v4_addr = {};
  v4_addr.sin_family = AF_UNIX;
  memcpy(&conn.open.addr, &v4_addr, sizeof(struct sockaddr_in));

  tracker.AddControlEvent(conn);

  CIDRBlock cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

  tracker.IterationPreTick(cidr, /*proc_parser*/ nullptr, /*connections*/ nullptr);
  EXPECT_EQ(ConnectionTracker::State::kDisabled, tracker.state());
}

namespace {

void SetIPv4RemoteAddr(std::string_view addr_str, struct socket_control_event_t* conn) {
  // Set an address that falls in the intra-cluster address range.
  struct sockaddr_in v4_addr = {};
  v4_addr.sin_family = AF_INET;
  uint16_t port = 123;
  v4_addr.sin_port = htons(port);
  // Note that address is outside of the CIDR block specified below.
  PL_CHECK_OK(ParseIPv4Addr(addr_str, &v4_addr.sin_addr));
  memcpy(&conn->open.addr, &v4_addr, sizeof(struct sockaddr_in));
}

void SetIPv6RemoteAddr(std::string_view addr_str, struct socket_control_event_t* conn) {
  // Set an address that falls in the intra-cluster address range.
  struct sockaddr_in6 v6_addr = {};
  v6_addr.sin6_family = AF_INET6;
  uint16_t port = 123;
  v6_addr.sin6_port = htons(port);
  // Note that address is outside of the CIDR block specified below.
  PL_CHECK_OK(ParseIPv6Addr(addr_str, &v6_addr.sin6_addr));
  memcpy(&conn->open.addr, &v6_addr, sizeof(struct sockaddr_in6));
}

}  // namespace

// Tests that tracker is disabled after mapping the addresses from IPv4 to IPv6.
TEST_F(ConnectionTrackerTest, TrackerDisabledAfterMapping) {
  {
    ConnectionTracker tracker;
    std::vector<http::Record> req_resp_pairs;

    struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
    conn.open.traffic_class.role = EndpointRole::kRoleClient;
    SetIPv6RemoteAddr("::ffff:1.2.3.4", &conn);

    tracker.AddControlEvent(conn);

    CIDRBlock cidr;
    ASSERT_OK(ParseCIDRBlock("1.2.3.4/14", &cidr));

    tracker.IterationPreTick(cidr, /*proc_parser*/ nullptr, /*connections*/ nullptr);
    EXPECT_EQ(ConnectionTracker::State::kDisabled, tracker.state());
  }
  {
    ConnectionTracker tracker;
    std::vector<http::Record> req_resp_pairs;

    struct socket_control_event_t conn = InitConn<kProtocolHTTP>();
    conn.open.traffic_class.role = EndpointRole::kRoleClient;
    SetIPv4RemoteAddr("1.2.3.4", &conn);

    tracker.AddControlEvent(conn);

    CIDRBlock cidr;
    ASSERT_OK(ParseCIDRBlock("::ffff:1.2.3.4/120", &cidr));

    tracker.IterationPreTick(cidr, /*proc_parser*/ nullptr, /*connections*/ nullptr);
    EXPECT_EQ(ConnectionTracker::State::kDisabled, tracker.state());
  }
}

}  // namespace stirling
}  // namespace pl
