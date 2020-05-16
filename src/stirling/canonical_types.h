#pragma once

#include <map>
#include <string_view>

#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {
namespace canonical_data_elements {

static const std::map<int64_t, std::string_view> kTraceSideDecoder =
    pl::EnumDefToMap<EndpointRole>();

constexpr DataElement kTime = {"time_", types::DataType::TIME64NS,
                               types::PatternType::METRIC_COUNTER,
                               "Timestamp when the data record was collected."};
constexpr DataElement kUPID = {
    "upid", types::DataType::UINT128, types::PatternType::GENERAL,
    "An opaque numeric ID that globally identify a running process inside the cluster."};
// TODO(PL-519): Use uint128 to represent IP addresses.
constexpr DataElement kRemoteAddr = {"remote_addr", types::DataType::STRING,
                                     types::PatternType::GENERAL,
                                     "IP address of the remote endpoint."};
constexpr DataElement kRemotePort = {"remote_port", types::DataType::INT64,
                                     types::PatternType::GENERAL, "Port of the remote endpoint."};
constexpr DataElement kTraceRole = {
    "trace_role", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
    "Side (client-or-server) where traffic was traced", &kTraceSideDecoder};

};  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace pl
