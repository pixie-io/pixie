#include "src/stirling/connection_stats.h"

#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {

namespace {

ConnectionStats::AggKey BuildAggKey(const upid_t& upid, const traffic_class_t& traffic_class,
                                    const SockAddr& remote_endpoint) {
  return {
      .upid = upid,
      // TODO(yzhao): Remote address might not be resolved yet. That causes imprecise stats.
      // Add code in address resolution to update stats after resolution is done.
      .remote_addr = remote_endpoint.AddrStr(),
      // Set port to 0 if this event is from a server process.
      // This avoids creating excessive amount of records from changing ports of K8s services.
      .remote_port = traffic_class.role == kRoleServer ? 0 : remote_endpoint.port(),
  };
}

}  // namespace

void ConnectionStats::AddConnOpenEvent(const ConnectionTracker& tracker) {
  const conn_id_t& conn_id = tracker.conn_id();
  const traffic_class_t& tcls = tracker.traffic_class();
  const SockAddr& remote_endpoint = tracker.remote_endpoint();
  const bool is_open = true;

  RecordConn(conn_id, tcls, remote_endpoint, is_open);
}

void ConnectionStats::AddConnCloseEvent(const ConnectionTracker& tracker) {
  const conn_id_t& conn_id = tracker.conn_id();
  const traffic_class_t& tcls = tracker.traffic_class();
  const auto& remote_endpoint = tracker.remote_endpoint();
  const bool is_open = false;

  RecordConn(conn_id, tcls, remote_endpoint, is_open);
}

void ConnectionStats::AddDataEvent(const ConnectionTracker& tracker, const SocketDataEvent& event) {
  const upid_t& upid = event.attr.conn_id.upid;
  const traffic_class_t& tcls = tracker.traffic_class();
  const TrafficDirection dir = event.attr.direction;
  const SockAddr& remote_endpoint = tracker.remote_endpoint();
  const size_t size = event.attr.msg_size;

  RecordData(upid, tcls, dir, remote_endpoint, size);
}

void ConnectionStats::RecordConn(const struct conn_id_t& conn_id,
                                 const struct traffic_class_t& traffic_class,
                                 const SockAddr& remote_endpoint, bool is_open) {
  AggKey key = BuildAggKey(conn_id.upid, traffic_class, remote_endpoint);

  if (is_open) {
    if (!known_conns_.contains(conn_id)) {
      auto& stats = agg_stats_[key];

      ++stats.conn_open;
      stats.traffic_class = traffic_class;
      stats.addr_family = remote_endpoint.family;
      known_conns_.insert(conn_id);
    }
  } else {
    auto iter = known_conns_.find(conn_id);
    if (iter != known_conns_.end()) {
      auto& stats = agg_stats_[key];

      ++stats.conn_close;
      stats.traffic_class = traffic_class;
      stats.addr_family = remote_endpoint.family;
      known_conns_.erase(iter);
    }
  }
}

void ConnectionStats::RecordData(const struct upid_t& upid,
                                 const struct traffic_class_t& traffic_class,
                                 TrafficDirection direction, const SockAddr& remote_endpoint,
                                 size_t size) {
  AggKey key = BuildAggKey(upid, traffic_class, remote_endpoint);
  auto& stats = agg_stats_[key];

  stats.traffic_class = traffic_class;

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
