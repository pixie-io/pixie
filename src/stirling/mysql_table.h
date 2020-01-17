#pragma once

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kMySQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        {"req_cmd", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
        "MySQL request command"},
        {"req_body", types::DataType::STRING, types::PatternType::GENERAL,
        "MySQL request body"},
        {"resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
        "MySQL response status code"},
        {"resp_body", types::DataType::STRING, types::PatternType::GENERAL,
        "MySQL resposne body"},
        {"latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
        "Request-response latency in nanoseconds"},
};
// clang-format on

static constexpr auto kMySQLTable = DataTableSchema("mysql_events", kMySQLElements);

constexpr int kMySQLUPIDIdx = kMySQLTable.ColIndex("upid");

}  // namespace stirling
}  // namespace pl
