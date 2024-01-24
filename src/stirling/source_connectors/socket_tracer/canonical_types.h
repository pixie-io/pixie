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

#include <map>
#include <string_view>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.hpp"

namespace px {
namespace stirling {
namespace canonical_data_elements {

// clang-format off
constexpr DataElement kRemoteAddr = {
    "remote_addr",
    "IP address of the remote endpoint.",
    types::DataType::STRING,
    types::SemanticType::ST_IP_ADDRESS,
    types::PatternType::GENERAL,
};

constexpr DataElement kRemotePort = {
    "remote_port",
    "Port of the remote endpoint.",
    types::DataType::INT64,
    types::SemanticType::ST_PORT,
    types::PatternType::GENERAL,
};

constexpr DataElement kLocalAddr = {
    "local_addr",
    "IP address of the local endpoint.",
    types::DataType::STRING,
    types::SemanticType::ST_IP_ADDRESS,
    types::PatternType::GENERAL,
};

constexpr DataElement kLocalPort = {
    "local_port",
    "Port of the local endpoint.",
    types::DataType::INT64,
    types::SemanticType::ST_PORT,
    types::PatternType::GENERAL,
};

constexpr DataElement kTraceRole = {
    "trace_role",
    "The role (client-or-server) of the process that owns the connections.",
    types::DataType::INT64,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL_ENUM,
    &kEndpointRoleDecoder,
};

constexpr DataElement kLatencyNS = {
    "latency",
    "Request-response latency.",
    types::DataType::INT64,
    types::SemanticType::ST_DURATION_NS,
    types::PatternType::METRIC_GAUGE,
};

constexpr DataElement kPXInfo = {
    "px_info_",
    "Pixie messages regarding the record (e.g. warnings)",
    types::DataType::STRING,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL,
};
// clang-format on

}  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace px
