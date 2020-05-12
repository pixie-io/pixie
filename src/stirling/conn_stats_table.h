#pragma once

#include "src/stirling/canonical_types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kConnStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        {"role", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
        "The role of the process that owns the connections."},
        {"protocol", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
        "The protocol of the traffic on the connections."},
        {"conn_open", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
        "The number of connections opened since the beginning of tracing."},
        {"conn_close", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
        "The number of connections closed since the beginning of tracing."},
        {"conn_active", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
        "The number of active connections"},
        {"bytes_sent", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
         "The number of bytes sent to the remote endpoint(s)."},
        {"bytes_recv", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
         "The number of bytes received from the remote endpoint(s)."},
#ifndef NDEBUG
        {"px_info_", types::DataType::STRING, types::PatternType::GENERAL,
         "Pixie messages regarding the record (e.g. warnings)"},
#endif
};
// clang-format on

static constexpr auto kConnStatsTable = DataTableSchema("conn_stats", kConnStatsElements);

}  // namespace stirling
}  // namespace pl
