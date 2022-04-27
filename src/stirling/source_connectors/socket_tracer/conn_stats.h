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

#pragma once

#include <string>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>

#include "src/shared/upid/upid.h"
#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

/**
 * Records the aggregated stats on all connections.
 */
class ConnStats {
 public:
  explicit ConnStats(ConnTrackersManager* conn_trackers_mgr = nullptr)
      : conn_trackers_mgr_(conn_trackers_mgr) {}

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
    traffic_protocol_t protocol = kProtocolUnknown;
    endpoint_role_t role = kRoleUnknown;
    SockAddrFamily addr_family = SockAddrFamily::kUnspecified;
    bool ssl = false;

    uint64_t conn_open = 0;
    uint64_t conn_close = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;

    // Keep track of whether this stats object has ever been previously reported.
    // Used to determine whether it should be reported in the future, after the connection
    // has closed.
    bool reported = false;

    // Last time this stats object was updated.
    // Used to indicate whether or not to skip exporting the current record.
    int last_update = 0;

    std::string ToString() const {
      return absl::Substitute(
          "[conn_open=$0 conn_close=$1 bytes_sent=$2 bytes_recv=$3 protocol=$4 role=$5]", conn_open,
          conn_close, bytes_sent, bytes_recv, magic_enum::enum_name(protocol),
          magic_enum::enum_name(role));
    }
  };

  /**
   * Iterates through all the trackers and updates the internal state with any newly collected
   * information that the tracker has received.
   *
   * @return A mutable reference to all the aggregated connection stats. The reference is mutable
   *         for the purposes of removing stats that are no longer needed.
   */
  absl::flat_hash_map<AggKey, Stats>& UpdateStats();

  bool Active(const Stats& stats) { return update_counter_ == stats.last_update; }

 private:
  absl::flat_hash_map<AggKey, Stats> agg_stats_;

  ConnTrackersManager* conn_trackers_mgr_ = nullptr;

  int update_counter_ = 0;
};

}  // namespace stirling
}  // namespace px
