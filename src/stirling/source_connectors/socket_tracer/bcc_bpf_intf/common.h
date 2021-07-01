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

#ifdef __cplusplus
#include <algorithm>
#include <map>
#include <string>

#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/common/base/enum_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#endif

// This file contains definitions that are shared between various kprobes and uprobes.

enum MessageType { kUnknown, kRequest, kResponse };

enum TrafficDirection {
  kEgress,
  kIngress,
};

// Protocol being used on a connection (HTTP, MySQL, etc.).
// PROTOCOL_LIST: Requires update on new protocols.
enum TrafficProtocol {
  kProtocolUnknown = 0,
  kProtocolHTTP,
  kProtocolHTTP2,
  kProtocolMySQL,
  kProtocolCQL,
  kProtocolPGSQL,
  kProtocolDNS,
  kProtocolRedis,
  kProtocolMongo,
  kProtocolKafka,
  kNumProtocols
};

#ifdef __cplusplus
static const std::map<int64_t, std::string_view> kTrafficProtocolDecoder =
    px::EnumDefToMap<TrafficProtocol>();
#endif

struct protocol_message_t {
  enum TrafficProtocol protocol;
  enum MessageType type;
};

#ifdef __cplusplus
inline auto TrafficProtocolEnumValues() {
  auto protocols_array = magic_enum::enum_values<TrafficProtocol>();

  // Strip off last element in protocols_array, which is not a real protocol.
  constexpr int kNumProtocols = magic_enum::enum_count<TrafficProtocol>() - 1;
  std::array<TrafficProtocol, kNumProtocols> protocols;
  std::copy(protocols_array.begin(), protocols_array.end() - 1, protocols.begin());
  return protocols;
}
#endif

// The direction of traffic expected on a probe.
// Values have single bit set, so that they could be used as bit masks.
// WARNING: Do not change the existing mappings (PxL scripts rely on them).
enum EndpointRole {
  kRoleClient = 1 << 0,
  kRoleServer = 1 << 1,
  kRoleUnknown = 1 << 2,
};

#ifdef __cplusplus
static const std::map<int64_t, std::string_view> kEndpointRoleDecoder =
    px::EnumDefToMap<EndpointRole>();
#endif

struct conn_id_t {
  // The unique identifier of the pid/tgid.
  struct upid_t upid;
  // The file descriptor to the opened network connection.
  int32_t fd;
  // Unique id of the conn_id (timestamp).
  uint64_t tsid;
};

#ifdef __cplusplus
inline std::string ToString(const conn_id_t& conn_id) {
  return absl::Substitute("[pid=$0 start_time_ticks=$1 fd=$2 gen=$3]", conn_id.upid.pid,
                          conn_id.upid.start_time_ticks, conn_id.fd, conn_id.tsid);
}

inline bool operator==(const struct conn_id_t& a, const struct conn_id_t& b) {
  return a.upid == b.upid && a.fd == b.fd && a.tsid == b.tsid;
}

inline bool operator!=(const struct conn_id_t& a, const struct conn_id_t& b) { return !(a == b); }
#endif

// Specifies the corresponding indexes of the entries of a per-cpu array.
enum ControlValueIndex {
  // This specify one pid to monitor. This is used during test to eliminate noise.
  // TODO(yzhao): We need a more robust mechanism for production use, which should be able to:
  // * Specify multiple pids up to a certain limit, let's say 1024.
  // * Support efficient lookup inside bpf to minimize overhead.
  kTargetTGIDIndex = 0,
  kStirlingTGIDIndex,
  kNumControlValues,
};
