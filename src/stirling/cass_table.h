#pragma once

#include <map>

#include "src/stirling/canonical_types.h"
#include "src/stirling/cql/types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

static const std::map<int64_t, std::string_view> kCQLReqOpDecoder =
    pl::EnumDefToMap<cass::RespOp>();
static const std::map<int64_t, std::string_view> kCQLRespOpDecoder =
    pl::EnumDefToMap<cass::RespOp>();

// clang-format off
static constexpr DataElement kCQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req_op", types::DataType::INT64, types::PatternType::GENERAL_ENUM, "Request opcode",
         &kCQLReqOpDecoder},
        {"req_body", types::DataType::STRING, types::PatternType::GENERAL, "Request body"},
        {"resp_op", types::DataType::INT64, types::PatternType::GENERAL_ENUM, "Response opcode",
         &kCQLRespOpDecoder},
        {"resp_body", types::DataType::STRING, types::PatternType::GENERAL, "Request body"},
        {"latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
         "Request-response latency in nanoseconds"},
#ifndef NDEBUG
        {"px_info_", types::DataType::STRING, types::PatternType::GENERAL,
         "Pixie messages regarding the record (e.g. warnings)"},
#endif
};
// clang-format on

static constexpr auto kCQLTable = DataTableSchema(
    "cql_events", kCQLElements, std::chrono::milliseconds{100}, std::chrono::milliseconds{1000});

static constexpr int kCQLTraceRoleIdx = kCQLTable.ColIndex("trace_role");
static constexpr int kCQLUPIDIdx = kCQLTable.ColIndex("upid");
static constexpr int kCQLReqOp = kCQLTable.ColIndex("req_op");
static constexpr int kCQLReqBody = kCQLTable.ColIndex("req_body");
static constexpr int kCQLRespOp = kCQLTable.ColIndex("resp_op");
static constexpr int kCQLRespBody = kCQLTable.ColIndex("resp_body");

}  // namespace stirling
}  // namespace pl
