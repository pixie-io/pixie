#pragma once

#include <string>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>

#include "src/shared/upid/upid.h"
#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/conn_tracker.h"

namespace pl {
namespace stirling {

/**
 * Records the aggregated stats on all ConnTracker objects.
 *
 * The APIs mirrors ConnTracker.
 */
class ConnStats {
 public:
  // AggKey is the ConnStats connection identifier; it is unique to a client-server pair.
  // The upid represents the local process and the remote addr+port represents the remote endpoint.
  // When the local process (upid) represents a servers, the remote port of the client will be
  // set to 0 by BuildAggKey() to collapse multiple connections from the same client to a server.
  struct AggKey {
    struct upid_t upid;
    std::string remote_addr;
    int remote_port;

    bool operator==(const AggKey& rhs) const {
      return upid.tgid == rhs.upid.tgid && upid.start_time_ticks == rhs.upid.start_time_ticks &&
             remote_addr == rhs.remote_addr && remote_port == rhs.remote_port;
    }

    template <typename H>
    friend H AbslHashValue(H h, const AggKey& key) {
      return H::combine(std::move(h), key.upid.tgid, key.upid.start_time_ticks, key.remote_addr,
                        key.remote_port);
    }

    std::string ToString() const {
      return absl::Substitute("[tgid=$0 addr=$1 port=$2]", upid.tgid, remote_addr, remote_port);
    }
  };

  struct Stats {
    traffic_class_t traffic_class = {};
    SockAddrFamily addr_family = SockAddrFamily::kUnspecified;

    uint64_t conn_open = 0;
    uint64_t conn_close = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;

    // Values of bytes_sent and bytes_recv of the previous record transfer.
    // Used to indicate whether or not to skip exporting the current record.
    // Initialize to -1 so an initial connection event with no data is reported.
    std::optional<uint64_t> prev_bytes_sent;
    std::optional<uint64_t> prev_bytes_recv;

    std::string ToString() const {
      return absl::Substitute(
          "[conn_open=$0 conn_close=$1 bytes_sent=$2 bytes_recv=$3 traffic_class=$4]", conn_open,
          conn_close, bytes_sent, bytes_recv, ::ToString(traffic_class));
    }
  };

  auto& mutable_agg_stats() { return agg_stats_; }

  void AddConnOpenEvent(const ConnTracker& tracker);
  void AddConnCloseEvent(const ConnTracker& tracker);
  void AddDataEvent(const ConnTracker& tracker, const SocketDataEvent& event);

  void RecordData(const struct upid_t& upid, const struct traffic_class_t& traffic_class,
                  TrafficDirection direction, const SockAddr& remote_endpoint, size_t size);

 private:
  void RecordConn(const struct conn_id_t& conn_id, const struct traffic_class_t& traffic_class,
                  const SockAddr& remote_endpoint, bool is_open);

  absl::flat_hash_map<AggKey, Stats> agg_stats_;
};

}  // namespace stirling
}  // namespace pl
