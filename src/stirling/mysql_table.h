#pragma once

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kMySQLElements[] = {
        {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
        {"upid", types::DataType::UINT128, types::PatternType::GENERAL},
        {"pid_start_time", types::DataType::INT64, types::PatternType::GENERAL},
        {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
        {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
        {"body", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"status", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
};
// clang-format on
static constexpr auto kMySQLTable = DataTableSchema("mysql_events", kMySQLElements);

}  // namespace stirling
}  // namespace pl
