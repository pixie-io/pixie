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

  conn_info_t InitConn(uint64_t ts_ns = 0) {
    conn_info_t conn_info{};
    conn_info.addr.sin6_family = AF_INET;
    conn_info.timestamp_ns = ts_ns;
    conn_info.conn_id.tgid = kPID;
    conn_info.conn_id.fd = kFD;
    conn_info.conn_id.generation = kPIDFDGeneration;
    conn_info.traffic_class.protocol = kProtocolHTTP;
    conn_info.traffic_class.role = kRoleRequestor;
    conn_info.rd_seq_num = 0;
    conn_info.wr_seq_num = 0;
    return conn_info;
  }

  SocketDataEvent InitSendEvent(std::string_view msg, uint64_t ts_ns) {
    SocketDataEvent event = InitDataEvent(kEventTypeSyscallSendEvent, msg, ts_ns);
    event.attr.seq_num = send_seq_num_;
    send_seq_num_++;
    return event;
  }

  SocketDataEvent InitRecvEvent(std::string_view msg, uint64_t ts_ns) {
    SocketDataEvent event = InitDataEvent(kEventTypeSyscallRecvEvent, msg, ts_ns);
    event.attr.seq_num = recv_seq_num_;
    recv_seq_num_++;
    return event;
  }

  SocketDataEvent InitDataEvent(EventType event_type, std::string_view msg, uint64_t ts_ns) {
    socket_data_event_t event = {};
    event.attr.event_type = event_type;
    event.attr.traffic_class.protocol = kProtocolHTTP;
    event.attr.traffic_class.role = kRoleRequestor;
    event.attr.timestamp_ns = ts_ns;
    event.attr.conn_id.tgid = kPID;
    event.attr.conn_id.fd = kFD;
    event.attr.conn_id.generation = kPIDFDGeneration;
    event.attr.msg_size = msg.size();
    msg.copy(event.msg, msg.size());
    return SocketDataEvent(&event);
  }

  conn_info_t InitClose(uint64_t ts_ns) {
    conn_info_t conn_info{};
    conn_info.timestamp_ns = ts_ns;
    conn_info.conn_id.tgid = kPID;
    conn_info.conn_id.fd = kFD;
    conn_info.conn_id.generation = kPIDFDGeneration;
    conn_info.rd_seq_num = recv_seq_num_;
    conn_info.wr_seq_num = send_seq_num_;
    return conn_info;
  }

  uint64_t send_seq_num_ = 0;
  uint64_t recv_seq_num_ = 0;
};

TEST_F(ConnectionTrackerTest, timestamp_test) {
  ConnectionTracker tracker;

  uint64_t time = 0;

  conn_info_t conn = InitConn(++time);
  SocketDataEvent event0 = InitSendEvent("event0", ++time);
  SocketDataEvent event1 = InitRecvEvent("event1", ++time);
  SocketDataEvent event2 = InitSendEvent("event2", ++time);
  SocketDataEvent event3 = InitRecvEvent("event3", ++time);
  SocketDataEvent event4 = InitSendEvent("event4", ++time);
  SocketDataEvent event5 = InitRecvEvent("event5", ++time);
  conn_info_t close_conn = InitClose(++time);

  EXPECT_EQ(0, tracker.last_bpf_timestamp_ns());
  tracker.AddConnOpenEvent(conn);
  EXPECT_EQ(1, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(event0);
  EXPECT_EQ(2, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(event1);
  EXPECT_EQ(3, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(event5);
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(event2);
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(event3);
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddDataEvent(event4);
  EXPECT_EQ(7, tracker.last_bpf_timestamp_ns());
  tracker.AddConnCloseEvent(close_conn);
  EXPECT_EQ(8, tracker.last_bpf_timestamp_ns());
}

TEST(DataStreamTest, CannotSwitchType) {
  DataStream stream;
  stream.ExtractMessages<HTTPMessage>(MessageType::kRequests);
  EXPECT_DEATH(stream.ExtractMessages<http2::Frame>(MessageType::kRequests),
               "ConnectionTracker cannot change the type it holds during runtime");
}

}  // namespace stirling
}  // namespace pl
