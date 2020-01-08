#pragma once

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kMySQLElements[] = {
        {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
        {"upid", types::DataType::UINT128, types::PatternType::GENERAL},
        {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
        {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
        {"req_cmd", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
        {"req_body", types::DataType::STRING, types::PatternType::GENERAL},
        {"resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
        {"resp_body", types::DataType::STRING, types::PatternType::GENERAL},
        {"latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
};
// clang-format on
static constexpr auto kMySQLTable = DataTableSchema("mysql_events", kMySQLElements);

}  // namespace stirling
}  // namespace pl
