#pragma once

#include "src/stirling/canonical_types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kCQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        {"req_op", types::DataType::INT64, types::PatternType::GENERAL_ENUM, "Request opcode"},
        {"req_body", types::DataType::STRING, types::PatternType::GENERAL, "Request body"},
        {"resp_op", types::DataType::INT64, types::PatternType::GENERAL_ENUM, "Response opcode"},
        {"resp_body", types::DataType::STRING, types::PatternType::GENERAL, "Request body"},
        {"latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
         "Request-response latency in nanoseconds"},
};
// clang-format on

static constexpr auto kCQLTable = DataTableSchema("cql_events", kCQLElements);

}  // namespace stirling
}  // namespace pl
