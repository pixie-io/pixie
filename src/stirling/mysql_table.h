#pragma once

#include <map>

#include "src/stirling/canonical_types.h"
#include "src/stirling/core/types.h"
#include "src/stirling/protocols/mysql/types.h"

namespace pl {
namespace stirling {

static const std::map<int64_t, std::string_view> kMySQLReqCmdDecoder =
    pl::EnumDefToMap<protocols::mysql::Command>();
static const std::map<int64_t, std::string_view> kMySQLRespStatusDecoder =
    pl::EnumDefToMap<protocols::mysql::RespStatus>();

// clang-format off
static constexpr DataElement kMySQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req_cmd", "MySQL request command",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kMySQLReqCmdDecoder},
        {"req_body", "MySQL request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_status", "MySQL response status code",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kMySQLRespStatusDecoder},
        {"resp_body", "MySQL response body",
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
