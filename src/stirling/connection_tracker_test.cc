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

  const std::string kHTTPReq =
      "GET /index.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kHTTPResp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";
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

TEST_F(ConnectionTrackerTest, lost_event_test) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent(kHTTPReq);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent(kHTTPReq);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent(kHTTPReq);
  std::unique_ptr<SocketDataEvent> req3 = InitSendEvent(kHTTPReq);
  std::unique_ptr<SocketDataEvent> req4 = InitSendEvent(kHTTPReq);
  std::unique_ptr<SocketDataEvent> req5 = InitSendEvent(kHTTPReq);

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
