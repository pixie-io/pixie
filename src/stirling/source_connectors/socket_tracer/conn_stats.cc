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

ConnStats::AggKey BuildAggKey(const upid_t& upid, const traffic_class_t& traffic_class,
                              const SockAddr& remote_endpoint) {
  // Both local UPID and remote endpoint must be fully specified.
  DCHECK_NE(upid.pid, 0);
  DCHECK_NE(upid.start_time_ticks, 0);
  DCHECK(remote_endpoint.family != SockAddrFamily::kUnspecified);
  DCHECK(traffic_class.role != kRoleUnknown);
  return {
      .upid = upid,
      .remote_addr = remote_endpoint.AddrStr(),
      // Set port to 0 if this event is from a server process.
      // This avoids creating excessive amount of records from changing ports of K8s services.
      .remote_port = traffic_class.role == kRoleServer ? 0 : remote_endpoint.port(),
  };
}

}  // namespace

void ConnStats::AddConnOpenEvent(const ConnTracker& tracker) {
  const conn_id_t& conn_id = tracker.conn_id();
  const traffic_class_t& tcls = tracker.traffic_class();
  const SockAddr& remote_endpoint = tracker.remote_endpoint();
  const bool is_open = true;

  RecordConn(conn_id, tcls, remote_endpoint, is_open);
}

void ConnStats::AddConnCloseEvent(const ConnTracker& tracker) {
  const conn_id_t& conn_id = tracker.conn_id();
  const traffic_class_t& tcls = tracker.traffic_class();
  const SockAddr& remote_endpoint = tracker.remote_endpoint();
  const bool is_open = false;

  RecordConn(conn_id, tcls, remote_endpoint, is_open);
}

void ConnStats::AddDataEvent(const ConnTracker& tracker, const SocketDataEvent& event) {
  const upid_t& upid = event.attr.conn_id.upid;
  const traffic_class_t& tcls = tracker.traffic_class();
  const TrafficDirection dir = event.attr.direction;
  const SockAddr& remote_endpoint = tracker.remote_endpoint();
  const size_t size = event.attr.msg_size;

  RecordData(upid, tcls, dir, remote_endpoint, size);
}

void ConnStats::RecordConn(const struct conn_id_t& conn_id,
                           const struct traffic_class_t& traffic_class,
                           const SockAddr& remote_endpoint, bool is_open) {
  AggKey key = BuildAggKey(conn_id.upid, traffic_class, remote_endpoint);

  auto& stats = agg_stats_[key];

  if (is_open) {
    ++stats.conn_open;
    stats.traffic_class = traffic_class;
    stats.addr_family = remote_endpoint.family;

  } else {
    ++stats.conn_close;
  }
}

void ConnStats::RecordData(const struct upid_t& upid, const struct traffic_class_t& traffic_class,
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
}  // namespace px
