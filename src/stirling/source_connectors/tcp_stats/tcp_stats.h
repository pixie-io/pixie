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
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>

#include "src/common/base/inet_utils.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

/**
 * Records the aggregated stats on all TCP connections.
 */
class TCPStats {
 public:
  // AggKey is the TCPStats connection identifier; it is unique to a client-server pair.
  // The upid represents the local process and the remote addr+port represents the remote endpoint.
  // When the local process (upid) represents a servers, the remote port of the client will be
  // set to 0 by BuildAggKey() to collapse multiple connections from the same client to a server.
  struct AggKey {
    struct upid_t upid;
    std::string local_addr;
    int local_port;
    std::string remote_addr;
    int remote_port;

    bool operator==(const AggKey& rhs) const {
      return upid.tgid == rhs.upid.tgid && upid.start_time_ticks == rhs.upid.start_time_ticks &&
             local_addr == rhs.local_addr && local_port == rhs.local_port &&
             remote_addr == rhs.remote_addr && remote_port == rhs.remote_port;
    }

    template <typename H>
    friend H AbslHashValue(H h, const AggKey& key) {
      return H::combine(std::move(h), key.upid.tgid, key.upid.start_time_ticks, key.local_addr,
                        key.local_port, key.remote_addr, key.remote_port);
    }

    std::string ToString() const {
      return absl::Substitute("[tgid=$0 local_addr=$1 local_port=$2 remote_addr=$3 remote_port=$4]",
                              upid.tgid, local_addr, local_port, remote_addr, remote_port);
    }
  };

  struct Stats {
    SockAddrFamily addr_family = SockAddrFamily::kUnspecified;

    uint64_t bytes_sent = 0;
    uint64_t bytes_recv = 0;
    uint64_t retransmissions = 0;

    std::string ToString() const {
      return absl::Substitute("[bytes_sent=$0 bytes_recv=$1 retransmissions=$2]", bytes_sent,
                              bytes_recv, retransmissions);
    }
  };

  /**
   * Iterates through all the trackers and updates the internal state with any newly collected
   * information that the tracker has received.
   *
   * @return A mutable reference to all the aggregated connection stats. The reference is mutable
   *         for the purposes of removing stats that are no longer needed.
   */
  absl::flat_hash_map<AggKey, Stats>* UpdateStats(const std::vector<tcp_event_t> events);

 private:
  absl::flat_hash_map<AggKey, Stats> tcp_agg_stats_;
};

}  // namespace stirling
}  // namespace px
