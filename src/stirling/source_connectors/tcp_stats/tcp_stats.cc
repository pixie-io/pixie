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

#include "src/stirling/source_connectors/tcp_stats/tcp_stats.h"

#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

namespace {

TcpStats::AggKey BuildAggKey(const upid_t& upid, const SockAddr& remote_endpoint) {
  DCHECK_NE(upid.pid, 0U);
  DCHECK_NE(upid.start_time_ticks, 0U);
  return {
      .upid = upid,
      .remote_addr = remote_endpoint.AddrStr(),
      .remote_port = remote_endpoint.port(),
  };
}

}  // namespace

absl::flat_hash_map<TcpStats::AggKey, TcpStats::Stats>& TcpStats::UpdateStats(
    std::vector<tcp_event_t> events) {
  for (auto& event : events) {
    if (!(event.remote_addr.sa.sa_family == AF_INET ||
          event.remote_addr.sa.sa_family == AF_INET6)) {
      continue;
    }
    SockAddr addr;
    PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&event.remote_addr), &addr);
    TcpStats::AggKey key = BuildAggKey(event.upid, addr);
    auto& stats = tcp_agg_stats_[key];

    if (event.type == kUnknownEvent) {
      continue;
    } else if (event.type == kTcpTx) {
      stats.bytes_sent += event.size;
    } else if (event.type == kTcpRx) {
      stats.bytes_recv += event.size;
    } else if (event.type == kTcpRetransmissions) {
      stats.retransmissions += event.size;
    }
  }
  return tcp_agg_stats_;
}

}  // namespace stirling
}  // namespace px
