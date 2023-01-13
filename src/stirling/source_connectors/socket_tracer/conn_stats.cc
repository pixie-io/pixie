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

#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"

#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

namespace {

ConnStats::AggKey BuildAggKey(const upid_t& upid, endpoint_role_t role,
                              const SockAddr& remote_endpoint) {
  // Both local UPID and remote endpoint must be fully specified.
  DCHECK_NE(upid.pid, 0U);
  DCHECK_NE(upid.start_time_ticks, 0U);
  DCHECK(remote_endpoint.family != SockAddrFamily::kUnspecified);
  DCHECK(role != kRoleUnknown);
  return {
      .upid = upid,
      .remote_addr = remote_endpoint.AddrStr(),
      // Set port to 0 if this event is from a server process.
      // This avoids creating excessive amount of records from changing ports of K8s services.
      .remote_port = role == kRoleServer ? 0 : remote_endpoint.port(),
  };
}

}  // namespace

absl::flat_hash_map<ConnStats::AggKey, ConnStats::Stats>& ConnStats::UpdateStats() {
  ++update_counter_;

  for (const auto& tracker : conn_trackers_mgr_->active_trackers()) {
    if (tracker->IsZombie()) {
      tracker->MarkFinalConnStatsReported();
    }

    if (!(tracker->remote_endpoint().family == SockAddrFamily::kIPv4 ||
          tracker->remote_endpoint().family == SockAddrFamily::kIPv6) ||
        tracker->role() == kRoleUnknown) {
      continue;
    }

    auto& conn_stats = tracker->conn_stats();
    int conn_open = conn_stats.OpenSinceLastRead();
    int conn_close = conn_stats.CloseSinceLastRead();
    int bytes_recv = conn_stats.BytesRecvSinceLastRead();
    int bytes_sent = conn_stats.BytesSentSinceLastRead();

    AggKey key = BuildAggKey(tracker->conn_id().upid, tracker->role(), tracker->remote_endpoint());
    auto& stats = agg_stats_[key];

    stats.addr_family = tracker->remote_endpoint().family;
    stats.role = tracker->role();
    stats.protocol = tracker->protocol();
    stats.ssl = tracker->ssl();
    stats.conn_open += conn_open;
    stats.conn_close += conn_close;
    stats.bytes_recv += bytes_recv;
    stats.bytes_sent += bytes_sent;

    stats.last_update = update_counter_;
  }

  return agg_stats_;
}

}  // namespace stirling
}  // namespace px
