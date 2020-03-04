#pragma once

#include <sys/socket.h>

#include <memory>
#include <string>

namespace pl {
namespace stirling {
namespace testing {

class Clock {
 public:
  virtual uint64_t now() = 0;
  virtual ~Clock() = default;
};

class RealClock : public Clock {
 public:
  uint64_t now() { return std::chrono::steady_clock::now().time_since_epoch().count(); }
};

class MockClock : public Clock {
 public:
  uint64_t now() { return ++t_; }

 private:
  uint64_t t_ = 0;
};

constexpr uint32_t kPID = 12345;
constexpr uint32_t kFD = 3;
constexpr uint64_t kPIDStartTimeTicks = 112358;

// Convenience functions and predefined data for generating events expected from BPF socket probes.
class EventGenerator {
 public:
  explicit EventGenerator(Clock* clock) : clock_(clock) {}

  template <TrafficProtocol TProtocol>
  struct socket_control_event_t InitConn() {
    struct socket_control_event_t conn_event {};
    conn_event.type = kConnOpen;
    conn_event.open.timestamp_ns = clock_->now();
    conn_event.open.conn_id.upid.pid = kPID;
    conn_event.open.conn_id.fd = kFD;
    conn_event.open.conn_id.generation = ++generation_;
    conn_event.open.conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
    conn_event.open.addr.sin6_family = AF_INET;
    conn_event.open.traffic_class.protocol = TProtocol;
    conn_event.open.traffic_class.role =
        (TProtocol == kProtocolHTTP2Uprobe) ? kRoleUnknown : kRoleClient;
    return conn_event;
  }

  template <TrafficProtocol TProtocol>
  std::unique_ptr<SocketDataEvent> InitSendEvent(std::string_view msg) {
    return InitDataEvent<TProtocol>(TrafficDirection::kEgress, send_seq_num_++, msg);
  }

  template <TrafficProtocol TProtocol>
  std::unique_ptr<SocketDataEvent> InitRecvEvent(std::string_view msg) {
    return InitDataEvent<TProtocol>(TrafficDirection::kIngress, recv_seq_num_++, msg);
  }

  template <TrafficProtocol TProtocol>
  std::unique_ptr<SocketDataEvent> InitDataEvent(TrafficDirection direction, uint64_t seq_num,
                                                 std::string_view msg) {
    socket_data_event_t event = {};
    event.attr.direction = direction;
    event.attr.traffic_class.protocol = TProtocol;
    event.attr.traffic_class.role = kRoleClient;
    event.attr.return_timestamp_ns = clock_->now();
    event.attr.conn_id.upid.pid = kPID;
    event.attr.conn_id.fd = kFD;
    event.attr.conn_id.generation = generation_;
    event.attr.conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
    event.attr.seq_num = seq_num;
    event.attr.msg_size = msg.size();
    msg.copy(event.msg, msg.size());
    return std::make_unique<SocketDataEvent>(&event);
  }

  socket_control_event_t InitClose() {
    struct socket_control_event_t close_event {};
    close_event.type = kConnClose;
    close_event.close.timestamp_ns = clock_->now();
    close_event.close.conn_id.upid.pid = kPID;
    close_event.close.conn_id.fd = kFD;
    close_event.close.conn_id.generation = generation_;
    close_event.close.conn_id.upid.start_time_ticks = kPIDStartTimeTicks;
    close_event.close.rd_seq_num = recv_seq_num_;
    close_event.close.wr_seq_num = send_seq_num_;
    return close_event;
  }

 private:
  Clock* clock_;
  uint32_t generation_ = 0;
  uint64_t send_seq_num_ = 0;
  uint64_t recv_seq_num_ = 0;
};

constexpr std::string_view kHTTPReq0 =
    "GET /index.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

constexpr std::string_view kHTTPResp0 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 5\r\n"
    "\r\n"
    "pixie";

constexpr std::string_view kHTTPReq1 =
    "GET /foo.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

constexpr std::string_view kHTTPResp1 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "foo";

constexpr std::string_view kHTTPReq2 =
    "GET /bar.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
    "\r\n";

constexpr std::string_view kHTTPResp2 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 3\r\n"
    "\r\n"
    "bar";

constexpr std::string_view kHTTPUpgradeReq =
    "GET /index.html HTTP/1.1\r\n"
    "Host: www.pixielabs.ai\r\n"
    "Connection: Upgrade\r\n"
    "Upgrade: websocket\r\n"
    "\r\n";

constexpr std::string_view kHTTPUpgradeResp =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "\r\n";

static constexpr std::string_view kHTTP2EndStreamHeadersFrame =
    ConstStringView("\x0\x0\x0\x1\x5\x0\x0\x0\x1");
static constexpr std::string_view kHTTP2EndStreamDataFrame =
    ConstStringView("\x0\x0\x0\x0\x1\x0\x0\x0\x1");

SocketDataEvent DataEventWithTimestamp(std::string_view msg, uint64_t timestamp) {
  SocketDataEvent event;
  event.attr.return_timestamp_ns = timestamp;
  event.attr.msg_size = msg.size();
  event.msg = msg;
  return event;
}

void SetIPv4RemoteAddr(struct socket_control_event_t* conn, std::string_view addr_str,
                       uint16_t port = 123) {
  // Set an address that falls in the intra-cluster address range.
  struct sockaddr_in v4_addr = {};
  v4_addr.sin_family = AF_INET;
  v4_addr.sin_port = htons(port);
  // Note that address is outside of the CIDR block specified below.
  PL_CHECK_OK(ParseIPv4Addr(addr_str, &v4_addr.sin_addr));
  memcpy(&conn->open.addr, &v4_addr, sizeof(struct sockaddr_in));
}

void SetIPv6RemoteAddr(struct socket_control_event_t* conn, std::string_view addr_str,
                       uint16_t port = 123) {
  // Set an address that falls in the intra-cluster address range.
  struct sockaddr_in6 v6_addr = {};
  v6_addr.sin6_family = AF_INET6;
  v6_addr.sin6_port = htons(port);
  // Note that address is outside of the CIDR block specified below.
  PL_CHECK_OK(ParseIPv6Addr(addr_str, &v6_addr.sin6_addr));
  memcpy(&conn->open.addr, &v6_addr, sizeof(struct sockaddr_in6));
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
