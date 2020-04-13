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

static constexpr auto kPGSqlTable = DataTableSchema("pgsql_events", kPGSqlElements);

constexpr int kPGSqlUPIDIdx = kPGSqlTable.ColIndex("upid");
constexpr int kPGSqlReqCmdIdx = kPGSqlTable.ColIndex("req_cmd");
constexpr int kPGSqlReqBodyIdx = kPGSqlTable.ColIndex("req_body");
constexpr int kPGSqlRespStatusIdx = kPGSqlTable.ColIndex("resp_status");
constexpr int kPGSqlRespBodyIdx = kPGSqlTable.ColIndex("resp_body");
constexpr int kPGSqlLatencyIdx = kPGSqlTable.ColIndex("latency_ns");

}  // namespace stirling
}  // namespace pl
