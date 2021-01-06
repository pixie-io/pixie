#pragma once

#include <map>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/types.h"
#include "src/stirling/protocols/redis/types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kRedisElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req", "Request",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp", "Response",
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

static constexpr auto kRedisTable =
    DataTableSchema("redis_events", kRedisElements, std::chrono::milliseconds{100},
                    std::chrono::milliseconds{1000});

constexpr int kRedisUPIDIdx = kRedisTable.ColIndex("upid");
constexpr int kRedisReqIdx = kRedisTable.ColIndex("req");
constexpr int kRedisRespIdx = kRedisTable.ColIndex("resp");

}  // namespace stirling
}  // namespace pl
