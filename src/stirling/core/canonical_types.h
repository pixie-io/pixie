#pragma once

#include <map>
#include <string_view>

#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/core/types.h"

namespace pl {
namespace stirling {
namespace canonical_data_elements {

static const std::map<int64_t, std::string_view> kTraceSideDecoder =
    pl::EnumDefToMap<EndpointRole>();

// clang-format off

constexpr DataElement kTime = {
    "time_",
    "Timestamp when the data record was collected.",
    types::DataType::TIME64NS,
    types::SemanticType::ST_NONE,
    types::PatternType::METRIC_COUNTER};

constexpr DataElement kUPID = {
    "upid",
    "An opaque numeric ID that globally identify a running process inside the cluster.",
    types::DataType::UINT128,
    types::SemanticType::ST_UPID,
    types::PatternType::GENERAL};

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
    "Side (client-or-server) where traffic was traced",
    types::DataType::INT64,
    types::SemanticType::ST_NONE,
    types::PatternType::GENERAL_ENUM,
    &kTraceSideDecoder};

// clang-format on

};  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace pl
