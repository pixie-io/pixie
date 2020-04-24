#pragma once

#include "src/stirling/canonical_types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kPGSQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        {"req", types::DataType::STRING, types::PatternType::GENERAL,
        "PostgreSQL request body"},
        {"resp", types::DataType::STRING, types::PatternType::GENERAL,
        "PostgreSQL response body"},
        {"latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
        "Request-response latency in nanoseconds"},
};
// clang-format on

static constexpr auto kPGSQLTable = DataTableSchema("pgsql_events", kPGSQLElements);

constexpr int kPGSQLUPIDIdx = kPGSQLTable.ColIndex("upid");
constexpr int kPGSQLReqIdx = kPGSQLTable.ColIndex("req");
constexpr int kPGSQLRespIdx = kPGSQLTable.ColIndex("resp");
constexpr int kPGSQLLatencyIdx = kPGSQLTable.ColIndex("latency_ns");

}  // namespace stirling
}  // namespace pl
