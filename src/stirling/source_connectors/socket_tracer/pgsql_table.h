#pragma once

#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kPGSQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req", "PostgreSQL request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp", "PostgreSQL response body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"latency_ns", "Request-response latency in nanoseconds",
         types::DataType::INT64,
         types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_GAUGE},
#ifndef NDEBUG
        {"px_info_", "Pixie messages regarding the record (e.g. warnings)",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
#endif
};
// clang-format on

static constexpr auto kPGSQLTable =
    DataTableSchema("pgsql_events", kPGSQLElements, std::chrono::milliseconds{100},
                    std::chrono::milliseconds{1000});

constexpr int kPGSQLUPIDIdx = kPGSQLTable.ColIndex("upid");
constexpr int kPGSQLReqIdx = kPGSQLTable.ColIndex("req");
constexpr int kPGSQLRespIdx = kPGSQLTable.ColIndex("resp");
constexpr int kPGSQLLatencyIdx = kPGSQLTable.ColIndex("latency_ns");

}  // namespace stirling
}  // namespace pl
