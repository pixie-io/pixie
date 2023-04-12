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

TCPStats::AggKey BuildAggKey(const upid_t& upid, const SockAddr& local_endpoint,
                             const SockAddr& remote_endpoint) {
  return {
      .upid = upid,
      .local_addr = local_endpoint.AddrStr(),
      .local_port = local_endpoint.port(),
      .remote_addr = remote_endpoint.AddrStr(),
      .remote_port = remote_endpoint.port(),
  };
}

}  // namespace

absl::flat_hash_map<TCPStats::AggKey, TCPStats::Stats>* TCPStats::UpdateStats(
    const std::vector<tcp_event_t> events) {
  for (auto& event : events) {
    SockAddr laddr, raddr;
    auto la = event.local_addr;
    auto ra = event.remote_addr;
    PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&la), &laddr);
    PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&ra), &raddr);
    TCPStats::AggKey key = BuildAggKey(event.upid, laddr, raddr);
    auto& stats = tcp_agg_stats_[key];

    switch (event.type) {
      case kUnknownEvent:
        continue;
      case kTCPTx:
        stats.bytes_sent += event.size;
        continue;
      case kTCPRx:
        stats.bytes_recv += event.size;
        continue;
      case kTCPRetransmissions:
        stats.retransmissions += event.size;
        continue;
      default:
        continue;
    }
  }
  return &tcp_agg_stats_;
}

}  // namespace stirling
}  // namespace px
