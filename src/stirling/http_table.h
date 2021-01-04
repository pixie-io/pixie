#pragma once

#include <map>

#include "src/stirling/canonical_types.h"
#include "src/stirling/core/types.h"

namespace pl {
namespace stirling {

enum class HTTPContentType {
  kUnknown = 0,
  kJSON = 1,
  // We use gRPC instead of PB to be consistent with the wording used in gRPC.
  kGRPC = 2,
};

static const std::map<int64_t, std::string_view> kHTTPContentTypeDecoder =
    pl::EnumDefToMap<HTTPContentType>();

// clang-format off
constexpr DataElement kHTTPElements[] = {
    canonical_data_elements::kTime,
    canonical_data_elements::kUPID,
    canonical_data_elements::kRemoteAddr,
    canonical_data_elements::kRemotePort,
    canonical_data_elements::kTraceRole,
    {"http_major_version", "HTTP major version, can be 1 or 2",
     types::DataType::INT64,
     types::SemanticType::ST_NONE,
     types::PatternType::GENERAL_ENUM},
    {"http_minor_version", "HTTP minor version, HTTP1 uses 1, HTTP2 set this value to 0",
     types::DataType::INT64,
     types::SemanticType::ST_NONE,
     types::PatternType::GENERAL_ENUM},
    {"http_content_type", "Type of the HTTP payload, can be JSON or protobuf",
     types::DataType::INT64,
     types::SemanticType::ST_NONE,
     types::PatternType::GENERAL_ENUM,
     &kHTTPContentTypeDecoder},
    {"http_req_headers", "Request headers in JSON format",
     types::DataType::STRING,
     types::SemanticType::ST_NONE,
     types::PatternType::STRUCTURED},
    {"http_req_method", "HTTP request method (e.g. GET, POST, ...)",
     types::DataType::STRING,
     types::SemanticType::ST_HTTP_REQ_METHOD,
     types::PatternType::GENERAL_ENUM},
    {"http_req_path", "Request path",
     types::DataType::STRING,
     types::SemanticType::ST_NONE,
     types::PatternType::STRUCTURED},
    {"http_req_body", "Request body in JSON format",
     types::DataType::STRING,
     types::SemanticType::ST_NONE,
     types::PatternType::STRUCTURED},
    {"http_req_body_size", "Request body size (before any truncation)",
     types::DataType::INT64,
     types::SemanticType::ST_BYTES,
     types::PatternType::METRIC_GAUGE},
    {"http_resp_headers", "Response headers in JSON format",
     types::DataType::STRING,
     types::SemanticType::ST_NONE,
     types::PatternType::STRUCTURED},
    {"http_resp_status", "HTTP response status code",
     types::DataType::INT64,
     types::SemanticType::ST_HTTP_RESP_STATUS,
     types::PatternType::GENERAL_ENUM},
    {"http_resp_message", "HTTP response status text (e.g. OK, Not Found, ...)",
     types::DataType::STRING,
     types::SemanticType::ST_HTTP_RESP_MESSAGE,
     types::PatternType::STRUCTURED},
    {"http_resp_body", "Response body in JSON format",
     types::DataType::STRING,
     types::SemanticType::ST_NONE,
     types::PatternType::STRUCTURED},
    {"http_resp_body_size", "Response body size (before any truncation)",
     types::DataType::INT64,
     types::SemanticType::ST_BYTES,
     types::PatternType::METRIC_GAUGE},
    // TODO(yzhao): Rename this to latency_ns and consolidate into canonical_types.h.
    {"http_resp_latency_ns", "Request-response latency in nanoseconds",
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

constexpr auto kHTTPTable = DataTableSchema(
    "http_events", kHTTPElements, std::chrono::milliseconds{100}, std::chrono::milliseconds{1000});

constexpr int kHTTPTimeIdx = kHTTPTable.ColIndex("time_");
constexpr int kHTTPUPIDIdx = kHTTPTable.ColIndex("upid");
constexpr int kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
constexpr int kHTTPRemotePortIdx = kHTTPTable.ColIndex("remote_port");
constexpr int kHTTPTraceRoleIdx = kHTTPTable.ColIndex("trace_role");
constexpr int kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
constexpr int kHTTPMinorVersionIdx = kHTTPTable.ColIndex("http_minor_version");
constexpr int kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
constexpr int kHTTPReqHeadersIdx = kHTTPTable.ColIndex("http_req_headers");
constexpr int kHTTPReqMethodIdx = kHTTPTable.ColIndex("http_req_method");
constexpr int kHTTPReqPathIdx = kHTTPTable.ColIndex("http_req_path");
constexpr int kHTTPReqBodyIdx = kHTTPTable.ColIndex("http_req_body");
constexpr int kHTTPReqBodySizeIdx = kHTTPTable.ColIndex("http_req_body_size");
constexpr int kHTTPRespHeadersIdx = kHTTPTable.ColIndex("http_resp_headers");
constexpr int kHTTPRespStatusIdx = kHTTPTable.ColIndex("http_resp_status");
constexpr int kHTTPRespMessageIdx = kHTTPTable.ColIndex("http_resp_message");
constexpr int kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");
constexpr int kHTTPRespBodySizeIdx = kHTTPTable.ColIndex("http_resp_body_size");
constexpr int kHTTPLatencyIdx = kHTTPTable.ColIndex("http_resp_latency_ns");

}  // namespace stirling
}  // namespace pl
