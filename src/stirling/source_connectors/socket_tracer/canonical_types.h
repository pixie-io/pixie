#pragma once

#include <map>
#include <string_view>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

namespace pl {
namespace stirling {
namespace canonical_data_elements {

// clang-format off

// TODO(PL-519): Use uint128 to represent IP addresses.
constexpr DataElement kRemoteAddr = {
    "remote_addr",
    "IP address of the remote endpoint.",
    types::DataType::STRING,
    types::SemanticType::ST_IP_ADDRESS,
    types::PatternType::GENERAL};

constexpr DataElement kRemotePort = {
    "remote_port",
    "Port of the remote endpoint.",
    types::DataType::INT64,
    types::SemanticType::ST_PORT,
    types::PatternType::GENERAL};

constexpr DataElement kTraceRole = {
    "trace_role",
    "The role (client-or-server) of the process that owns the connections.",
    types::DataType::INT64,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL_ENUM,
    &kEndpointRoleDecoder};

constexpr DataElement kLatencyNS = {
    "latency",
    "Request-response latency.",
    types::DataType::INT64,
    types::SemanticType::ST_DURATION_NS,
    types::PatternType::METRIC_GAUGE};

// clang-format on

}  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace pl
