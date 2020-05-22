#pragma once

#include <deque>
#include <string>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>

#include "src/shared/metadata/base_types.h"
#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"
#include "src/stirling/common/go_grpc_types.h"
#include "src/stirling/common/socket_trace.h"
#include "src/stirling/connection_tracker.h"

namespace pl {
namespace stirling {

/**
 * Records the aggregated stats on all ConnectionTracker objects.
 *
 * The APIs mirrors ConnectionTracker.
 */
class ConnectionStats {
 public:
  // AggKey ideally should be unique to individual service instances. Such that the aggregated
  // metrics from an AggKey reflects a service instance, which then indicates the load on that
  // service instance.
  //
  // AggKey approximates that by combining upid and protocol.
  //
  // TODO(yzhao): One way to improve is to resolve the local port of the connections, and replace
  // protocol with local port number.
  struct AggKey {
    struct upid_t upid;
    struct traffic_class_t traffic_class;
    // This is empty for server role.
    std::string remote_addr;
    int remote_port;

    bool operator==(const AggKey& rhs) const {
      return upid.tgid == rhs.upid.tgid && upid.start_time_ticks == rhs.upid.start_time_ticks &&
             traffic_class.protocol == rhs.traffic_class.protocol &&
             traffic_class.role == rhs.traffic_class.role && remote_addr == rhs.remote_addr &&
             remote_port == rhs.remote_port;
    }

    template <typename H>
    friend H AbslHashValue(H h, const AggKey& key) {
      return H::combine(std::move(h), key.upid.tgid, key.upid.start_time_ticks,
                        key.traffic_class.protocol, key.traffic_class.role, key.remote_addr,
                        key.remote_port);
    }

    std::string DebugString() const {
      return absl::Substitute("[tgid=$0 protocol=$1 role=$2 addr=$3 port=$4]", upid.tgid,
                              traffic_class.protocol, traffic_class.role, remote_addr, remote_port);
    }
  };

  struct Stats {
    uint64_t conn_open = 0;
    uint64_t conn_close = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;

    std::string DebugString() const {
      return absl::Substitute("[conn_open=$0 conn_close=$1 bytes_sent=$2 bytes_recv=$3]", conn_open,
                              conn_close, bytes_sent, bytes_recv);
    }
  };

  const auto& agg_stats() const { return agg_stats_; }

  void AddControlEvent(const socket_control_event_t& event, const ConnectionTracker& tracker);
  void AddConnCloseEvent(const ConnectionTracker& tracker);
  void AddDataEvent(const ConnectionTracker& tracker, const SocketDataEvent& event);

  // TODO(yzhao): Handle HTTP2 events.

 private:
  void RecordConn(const struct upid_t& upid, const struct traffic_class_t& traffic_class,
                  std::string_view remote_addr, int remote_port, bool is_open);
  void RecordData(const struct upid_t& upid, const struct traffic_class_t& traffic_class,
                  TrafficDirection direction, std::string_view remote_addr, int remote_port,
                  size_t size);

  absl::flat_hash_map<AggKey, Stats> agg_stats_;
  absl::flat_hash_set<struct conn_id_t> known_conns_;
};

}  // namespace stirling
}  // namespace pl
