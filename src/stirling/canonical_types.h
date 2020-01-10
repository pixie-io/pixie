#pragma once

#include <string_view>

#include "src/stirling/types.h"

namespace pl {
namespace stirling {
namespace canonical_data_elements {

constexpr DataElement kTime = {"time_", types::DataType::TIME64NS,
                               types::PatternType::METRIC_COUNTER,
                               "Timestamp when the data record was collected."};
constexpr DataElement kUPID = {
    "upid", types::DataType::UINT128, types::PatternType::GENERAL,
    "An opaque numeric ID that globally identify a running process inside the cluster."};
constexpr DataElement kRemoteAddr = {"remote_addr", types::DataType::STRING,
                                     types::PatternType::GENERAL,
                                     "IP address of the remote endpoint."};
constexpr DataElement kRemotePort = {"remote_port", types::DataType::INT64,
                                     types::PatternType::GENERAL, "Port of the remote endpoint."};

}  // namespace canonical_data_elements
}  // namespace stirling
}  // namespace pl
