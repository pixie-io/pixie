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

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.hpp"

#include <algorithm>
#include <map>
#include <string>

#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/common/base/enum_utils.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

constexpr int kNumProtocols = magic_enum::enum_count<traffic_protocol_t>();

static const std::map<int64_t, std::string_view> kTrafficProtocolDecoder =
    px::EnumDefToMap<traffic_protocol_t>();

static const std::map<int64_t, std::string_view> kEndpointRoleDecoder =
    px::EnumDefToMap<endpoint_role_t>();

inline std::string ToString(const conn_id_t& conn_id) {
  return absl::Substitute("[upid=$0:$1 fd=$2 gen=$3]", conn_id.upid.pid,
                          conn_id.upid.start_time_ticks, conn_id.fd, conn_id.tsid);
}

inline bool operator==(const struct conn_id_t& a, const struct conn_id_t& b) {
  return a.upid == b.upid && a.fd == b.fd && a.tsid == b.tsid;
}

inline bool operator!=(const struct conn_id_t& a, const struct conn_id_t& b) { return !(a == b); }
