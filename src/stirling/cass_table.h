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

static constexpr int kCQLUPIDIdx = kCQLTable.ColIndex("upid");
static constexpr int kCQLReqOp = kCQLTable.ColIndex("req_op");
static constexpr int kCQLReqBody = kCQLTable.ColIndex("req_body");
static constexpr int kCQLRespOp = kCQLTable.ColIndex("resp_op");
static constexpr int kCQLRespBody = kCQLTable.ColIndex("resp_body");

}  // namespace stirling
}  // namespace pl
