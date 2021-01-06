#pragma once

#include <map>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/types.h"
#include "src/stirling/protocols/cql/types.h"

namespace pl {
namespace stirling {

static const std::map<int64_t, std::string_view> kCQLReqOpDecoder =
    pl::EnumDefToMap<protocols::cass::RespOp>();
static const std::map<int64_t, std::string_view> kCQLRespOpDecoder =
    pl::EnumDefToMap<protocols::cass::RespOp>();

// clang-format off
static constexpr DataElement kCQLElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req_op", "Request opcode",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kCQLReqOpDecoder},
        {"req_body", "Request body",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_op", "Response opcode",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kCQLRespOpDecoder},
        {"resp_body", "Request body",
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
