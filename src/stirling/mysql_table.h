#pragma once

#include <map>

#include "src/stirling/canonical_types.h"
#include "src/stirling/mysql/types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

static const std::map<int64_t, std::string_view> kMySQLReqCmdDecoder =
    pl::EnumDefToMap<mysql::Command>();
static const std::map<int64_t, std::string_view> kMySQLRespStatusDecoder =
    pl::EnumDefToMap<mysql::RespStatus>();

// clang-format off
static constexpr DataElement kMySQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req_cmd", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
        "MySQL request command", &kMySQLReqCmdDecoder},
        {"req_body", types::DataType::STRING, types::PatternType::GENERAL,
        "MySQL request body"},
        {"resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
        "MySQL response status code", &kMySQLRespStatusDecoder},
        {"resp_body", types::DataType::STRING, types::PatternType::GENERAL,
        "MySQL response body"},
        {"latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
        "Request-response latency in nanoseconds"},
#ifndef NDEBUG
        {"px_info_", types::DataType::STRING, types::PatternType::GENERAL,
         "Pixie messages regarding the record (e.g. warnings)"},
#endif
};
// clang-format on

static constexpr auto kMySQLTable =
    DataTableSchema("mysql_events", kMySQLElements, std::chrono::milliseconds{100},
                    std::chrono::milliseconds{1000});

constexpr int kMySQLTimeIdx = kMySQLTable.ColIndex("time_");
constexpr int kMySQLUPIDIdx = kMySQLTable.ColIndex("upid");
constexpr int kMySQLReqCmdIdx = kMySQLTable.ColIndex("req_cmd");
constexpr int kMySQLReqBodyIdx = kMySQLTable.ColIndex("req_body");
constexpr int kMySQLRespStatusIdx = kMySQLTable.ColIndex("resp_status");
constexpr int kMySQLRespBodyIdx = kMySQLTable.ColIndex("resp_body");
constexpr int kMySQLLatencyIdx = kMySQLTable.ColIndex("latency_ns");

}  // namespace stirling
}  // namespace pl
