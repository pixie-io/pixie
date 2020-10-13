#pragma once

#include <map>

#include "src/stirling/canonical_types.h"
#include "src/stirling/protocols/dns/types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kDNSElements[] = {
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

static constexpr auto kDNSTable = DataTableSchema(
    "dns_events", kDNSElements, std::chrono::milliseconds{100}, std::chrono::milliseconds{1000});

static constexpr int kDNSUPIDIdx = kDNSTable.ColIndex("upid");
static constexpr int kDNSReq = kDNSTable.ColIndex("req");
static constexpr int kDNSResp = kDNSTable.ColIndex("resp");

}  // namespace stirling
}  // namespace pl
