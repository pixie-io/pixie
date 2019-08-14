#include <gtest/gtest.h>
#include <sys/socket.h>

#include "src/stirling/connection_tracker.h"

namespace pl {
namespace stirling {

class ConnectionTrackerTest : public ::testing::Test {
 protected:
  // TODO(oazizi): Code related to creating BPF events is copied from SocketTraceConnectorTest.
  // Refactor to share.
  static constexpr uint32_t kPID = 12345;
  static constexpr uint32_t kFD = 3;
  static constexpr uint32_t kPIDFDGeneration = 2;

  void SetUp() {
    send_seq_num_ = 0;
    recv_seq_num_ = 0;
    current_ts_ns_ = 0;
  }

  conn_info_t InitConn() {
    conn_info_t conn_info{};
    conn_info.addr.sin6_family = AF_INET;
    conn_info.timestamp_ns = ++current_ts_ns_;
    conn_info.conn_id.pid = kPID;
    conn_info.conn_id.fd = kFD;
    conn_info.conn_id.generation = kPIDFDGeneration;
    conn_info.traffic_class.protocol = kProtocolHTTP;
    conn_info.traffic_class.role = kRoleRequestor;
    conn_info.rd_seq_num = 0;
    conn_info.wr_seq_num = 0;
    return conn_info;
  }

  std::unique_ptr<SocketDataEvent> InitSendEvent(std::string_view msg) {
    std::unique_ptr<SocketDataEvent> event = InitDataEvent(TrafficDirection::kEgress, msg);
    event->attr.seq_num = send_seq_num_;
    send_seq_num_++;
    return event;
  }

  std::unique_ptr<SocketDataEvent> InitRecvEvent(std::string_view msg) {
    std::unique_ptr<SocketDataEvent> event = InitDataEvent(TrafficDirection::kIngress, msg);
    event->attr.seq_num = recv_seq_num_;
    recv_seq_num_++;
    return event;
  }

  std::unique_ptr<SocketDataEvent> InitDataEvent(TrafficDirection direction, std::string_view msg) {
    socket_data_event_t event = {};
    event.attr.direction = direction;
    event.attr.traffic_class.protocol = kProtocolHTTP;
    event.attr.traffic_class.role = kRoleRequestor;
    event.attr.timestamp_ns = ++current_ts_ns_;
    event.attr.conn_id.pid = kPID;
    event.attr.conn_id.fd = kFD;
    event.attr.conn_id.generation = kPIDFDGeneration;
    event.attr.msg_size = msg.size();
    msg.copy(event.msg, msg.size());
    return std::make_unique<SocketDataEvent>(&event);
  }

  conn_info_t InitClose() {
    conn_info_t conn_info{};
    conn_info.timestamp_ns = ++current_ts_ns_;
    conn_info.conn_id.pid = kPID;
    conn_info.conn_id.fd = kFD;
    conn_info.conn_id.generation = kPIDFDGeneration;
    conn_info.rd_seq_num = recv_seq_num_;
    conn_info.wr_seq_num = send_seq_num_;
    return conn_info;
  }

  uint64_t send_seq_num_ = 0;
  uint64_t recv_seq_num_ = 0;
  uint64_t current_ts_ns_ = 0;

  const std::string kHTTPReq0 =
      "GET /index.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kHTTPResp0 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "pixie";

  const std::string kHTTPReq1 =
      "GET /foo.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kHTTPResp1 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";

  const std::string kHTTPReq2 =
      "GET /bar.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kHTTPResp2 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";
};

TEST_F(ConnectionTrackerTest, timestamp_test) {
  ConnectionTracker tracker;

  conn_info_t conn = InitConn();
  std::unique_ptr<SocketDataEvent> event0 = InitSendEvent("event0");
  std::unique_ptr<SocketDataEvent> event1 = InitRecvEvent("event1");
  std::unique_ptr<SocketDataEvent> event2 = InitSendEvent("event2");
  std::unique_ptr<SocketDataEvent> event3 = InitRecvEvent("event3");
  std::unique_ptr<SocketDataEvent> event4 = InitSendEvent("event4");
  std::unique_ptr<SocketDataEvent> event5 = InitRecvEvent("event5");
  conn_info_t close_conn = InitClose();

  EXPECT_EQ(0, tracker.last_bpf_timestamp_ns());
  tracker.AddConnOpenEvent(conn);
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
  tracker.AddConnCloseEvent(close_conn);
  EXPECT_EQ(8, tracker.last_bpf_timestamp_ns());
}

TEST_F(ConnectionTrackerTest, LostEvent) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req3 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req4 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req5 = InitSendEvent(kHTTPReq0);

  std::deque<HTTPMessage> requests;

  // Start off with no lost events.
  stream.AddEvent(std::move(req0));
  requests = stream.ExtractMessages<HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 1ULL);
  EXPECT_FALSE(stream.Stuck());

  // Now add some lost events - should get skipped over.
  PL_UNUSED(req1);  // Lost event.
  stream.AddEvent(std::move(req2));
  requests = stream.ExtractMessages<HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 2ULL);
  EXPECT_FALSE(stream.Stuck());

  // Some more requests, and another lost request (this time undetectable).
  stream.AddEvent(std::move(req3));
  PL_UNUSED(req4);
  requests = stream.ExtractMessages<HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 3ULL);
  EXPECT_FALSE(stream.Stuck());

  // Now the lost event should be detected.
  stream.AddEvent(std::move(req5));
  requests = stream.ExtractMessages<HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 4ULL);
  EXPECT_FALSE(stream.Stuck());
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingSimple) {
  ConnectionTracker tracker;

  conn_info_t conn = InitConn();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent(kHTTPResp2);
  conn_info_t close_conn = InitClose();

  tracker.AddConnOpenEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddConnCloseEvent(close_conn);

  std::vector<ReqRespPair<HTTPMessage>> req_resp_pairs;
  req_resp_pairs = tracker.ProcessMessages<ReqRespPair<HTTPMessage>>();

  ASSERT_EQ(3, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req_message.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp_message.http_msg_body, "pixie");

  EXPECT_EQ(req_resp_pairs[1].req_message.http_req_path, "/foo.html");
  EXPECT_EQ(req_resp_pairs[1].resp_message.http_msg_body, "foo");

  EXPECT_EQ(req_resp_pairs[2].req_message.http_req_path, "/bar.html");
  EXPECT_EQ(req_resp_pairs[2].resp_message.http_msg_body, "bar");
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingPipelined) {
  ConnectionTracker tracker;

  conn_info_t conn = InitConn();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent(kHTTPResp2);
  conn_info_t close_conn = InitClose();

  tracker.AddConnOpenEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(req1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddConnCloseEvent(close_conn);

  std::vector<ReqRespPair<HTTPMessage>> req_resp_pairs;
  req_resp_pairs = tracker.ProcessMessages<ReqRespPair<HTTPMessage>>();

  ASSERT_EQ(3, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req_message.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp_message.http_msg_body, "pixie");

  EXPECT_EQ(req_resp_pairs[1].req_message.http_req_path, "/foo.html");
  EXPECT_EQ(req_resp_pairs[1].resp_message.http_msg_body, "foo");

  EXPECT_EQ(req_resp_pairs[2].req_message.http_req_path, "/bar.html");
  EXPECT_EQ(req_resp_pairs[2].resp_message.http_msg_body, "bar");
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingSerializedMissingRequest) {
  ConnectionTracker tracker;

  conn_info_t conn = InitConn();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent(kHTTPResp2);
  conn_info_t close_conn = InitClose();

  tracker.AddConnOpenEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  PL_UNUSED(req1);  // Missing event.
  tracker.AddDataEvent(std::move(resp1));
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddConnCloseEvent(close_conn);

  std::vector<ReqRespPair<HTTPMessage>> req_resp_pairs;
  req_resp_pairs = tracker.ProcessMessages<ReqRespPair<HTTPMessage>>();

  ASSERT_EQ(3, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req_message.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp_message.http_msg_body, "pixie");

  EXPECT_EQ(req_resp_pairs[1].req_message.http_req_path, "-");
  EXPECT_EQ(req_resp_pairs[1].resp_message.http_msg_body, "foo");

  EXPECT_EQ(req_resp_pairs[2].req_message.http_req_path, "/bar.html");
  EXPECT_EQ(req_resp_pairs[2].resp_message.http_msg_body, "bar");
}

TEST_F(ConnectionTrackerTest, ReqRespMatchingSerializedMissingResponse) {
  ConnectionTracker tracker;

  conn_info_t conn = InitConn();
  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> resp0 = InitRecvEvent(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> resp1 = InitRecvEvent(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> resp2 = InitRecvEvent(kHTTPResp2);
  conn_info_t close_conn = InitClose();

  tracker.AddConnOpenEvent(conn);
  tracker.AddDataEvent(std::move(req0));
  tracker.AddDataEvent(std::move(resp0));
  tracker.AddDataEvent(std::move(req1));
  PL_UNUSED(req2);  // Missing event.
  tracker.AddDataEvent(std::move(req2));
  tracker.AddDataEvent(std::move(resp2));
  tracker.AddConnCloseEvent(close_conn);

  std::vector<ReqRespPair<HTTPMessage>> req_resp_pairs;

  req_resp_pairs = tracker.ProcessMessages<ReqRespPair<HTTPMessage>>();

  ASSERT_EQ(2, req_resp_pairs.size());

  EXPECT_EQ(req_resp_pairs[0].req_message.http_req_path, "/index.html");
  EXPECT_EQ(req_resp_pairs[0].resp_message.http_msg_body, "pixie");

  // Oops - expecting a mismatch? Yes! What else can we do?
  EXPECT_EQ(req_resp_pairs[1].req_message.http_req_path, "/foo.html");
  EXPECT_EQ(req_resp_pairs[1].resp_message.http_msg_body, "bar");

  // Final request sticks around waiting for a partner - who will never come!
  // TODO(oazizi): The close should be an indicator that the partner will never come.
}

TEST_F(ConnectionTrackerTest, stats_counter) {
  ConnectionTracker tracker;

  EXPECT_EQ(0, tracker.Stat(ConnectionTracker::CountStats::kDataEvent));

  tracker.IncrementStat(ConnectionTracker::CountStats::kDataEvent);
  EXPECT_EQ(1, tracker.Stat(ConnectionTracker::CountStats::kDataEvent));

  tracker.IncrementStat(ConnectionTracker::CountStats::kDataEvent);
  EXPECT_EQ(2, tracker.Stat(ConnectionTracker::CountStats::kDataEvent));
}

TEST(DataStreamTest, CannotSwitchType) {
  DataStream stream;
  stream.ExtractMessages<HTTPMessage>(MessageType::kRequest);
  EXPECT_DEATH(stream.ExtractMessages<http2::Frame>(MessageType::kRequest),
               "ConnectionTracker cannot change the type it holds during runtime");
}

}  // namespace stirling
}  // namespace pl
