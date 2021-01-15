#pragma once

#include <map>

#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"

namespace pl {
namespace stirling {

// clang-format off
static constexpr DataElement kDNSElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"req_header", "Request header",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"req_body", "Request",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_header", "Response header",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"resp_body", "Response",
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

static constexpr auto kDNSTable =
    DataTableSchema("dns_events", "DNS request-response pair events", kDNSElements,
                    std::chrono::milliseconds{100}, std::chrono::milliseconds{1000});

static constexpr int kDNSUPIDIdx = kDNSTable.ColIndex("upid");
static constexpr int kDNSReqHdrIdx = kDNSTable.ColIndex("req_header");
static constexpr int kDNSReqBodyIdx = kDNSTable.ColIndex("req_body");
static constexpr int kDNSRespHdrIdx = kDNSTable.ColIndex("resp_header");
static constexpr int kDNSRespBodyIdx = kDNSTable.ColIndex("resp_body");

}  // namespace stirling
}  // namespace pl
