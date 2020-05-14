#include "src/stirling/connection_stats.h"

#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/common/base/inet_utils.h"

namespace pl {
namespace stirling {

namespace {

ConnectionStats::AggKey BuildAggKey(const upid_t& upid, const traffic_class_t& traffic_class,
                                    std::string_view remote_addr, int remote_port) {
  DCHECK_NE(traffic_class.role, kRoleAll);
  return {
      .upid = upid,
      .traffic_class = traffic_class,
      // TODO(yzhao): Remote address might not be resolved yet. That causes imprecise stats.
      // Add code in address resolution to update stats after resolution is done.
      .remote_addr = std::string(remote_addr),
      .remote_port = remote_port,
  };
}

}  // namespace

void ConnectionStats::AddConnOpenEvent(const conn_event_t& event) {
  const upid_t& upid = event.conn_id.upid;
  const traffic_class_t tcls = {.protocol = kProtocolUnknown, .role = kRoleUnknown};
  SockAddr sock_addr = {};
  PopulateSockAddr(reinterpret_cast<const struct sockaddr*>(&event.addr), &sock_addr);
  std::string raddr = sock_addr.AddrStr();
  int rport = sock_addr.port;

  RecordConn(upid, tcls, raddr, rport, /*is_open*/ true);
}

void ConnectionStats::AddConnCloseEvent(const ConnectionTracker& tracker) {
  const conn_id_t& conn_id = tracker.conn_id();
  const traffic_class_t& tcls = tracker.traffic_class();
  const std::string raddr = tracker.remote_endpoint().AddrStr();
  const int rport = tracker.remote_endpoint().port;

  if (!known_conns_.contains(conn_id)) {
    // This can happen if we did not see the open event, and there is no data.
    VLOG(1) << absl::Substitute("Ignoring connection close [conn_id=$0]", ToString(conn_id));
    return;
  }
  RecordConn(conn_id.upid, tcls, raddr, rport, /*is_open*/ false);
}

void ConnectionStats::AddDataEvent(const ConnectionTracker& tracker, const SocketDataEvent& event) {
  // Save typing.
  const conn_id_t& conn_id = event.attr.conn_id;
  const upid_t& upid = event.attr.conn_id.upid;
  const traffic_class_t& tcls = event.attr.traffic_class;
  const TrafficDirection dir = event.attr.direction;
  const size_t size = event.attr.msg_size;
  // TODO(yzhao): Remote address might not be resolved yet. That causes imprecise stats.
  const std::string raddr = tracker.remote_endpoint().AddrStr();
  const int rport = tracker.remote_endpoint().port;

  RecordData(upid, tcls, dir, raddr, rport, size);
  if (!known_conns_.contains(conn_id)) {
    // Traffic class is known only after receiving at least one data event.
    RecordConn(upid, tcls, raddr, rport, /*is_open*/ true);
    known_conns_.insert(conn_id);
  }
}

void ConnectionStats::RecordConn(const struct upid_t& upid,
                                 const struct traffic_class_t& traffic_class,
                                 std::string_view remote_addr, int remote_port, bool is_open) {
  AggKey key = BuildAggKey(upid, traffic_class, remote_addr, remote_port);
  auto& stats = agg_stats_[key];

  if (is_open) {
    ++stats.conn_open;
  } else {
    ++stats.conn_close;
  }
}

void ConnectionStats::RecordData(const struct upid_t& upid,
                                 const struct traffic_class_t& traffic_class,
                                 TrafficDirection direction, std::string_view remote_addr,
                                 int remote_port, size_t size) {
  AggKey key = BuildAggKey(upid, traffic_class, remote_addr, remote_port);
  auto& stats = agg_stats_[key];

  switch (direction) {
    case TrafficDirection::kEgress:
      stats.bytes_sent += size;
      break;
    case TrafficDirection::kIngress:
      stats.bytes_recv += size;
      break;
  }
}

}  // namespace stirling
}  // namespace pl
