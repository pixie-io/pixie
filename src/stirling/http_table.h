#pragma once

#include "src/stirling/canonical_types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

// clang-format off
constexpr DataElement kHTTPElements[] = {
    canonical_data_elements::kTime,
    canonical_data_elements::kUPID,
    // TODO(PL-519): Eventually, use uint128 to represent IP addresses, as will be resolved in
    // the Jira issue.
    canonical_data_elements::kRemoteAddr,
    canonical_data_elements::kRemotePort,
    {"http_major_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
    "HTTP major version, can be 1 or 2"},
    {"http_minor_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
    "HTTP minor version, HTTP1 uses 1, HTTP2 set this value to 0"},
    {"http_content_type", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
    // TODO(yzhao): Add a map from enum to text, note that this has to be constexpr, the actual
    // mechanism might require some template trick.
    "Type of the HTTP payload, can be JSON or protobuf"},
    {"http_req_headers", types::DataType::STRING, types::PatternType::STRUCTURED,
    "Request headers in JSON format"},
    {"http_req_method", types::DataType::STRING, types::PatternType::GENERAL_ENUM,
    "HTTP request method (e.g. GET, POST, ...)"},
    {"http_req_path", types::DataType::STRING, types::PatternType::STRUCTURED,
    "Request path"},
    {"http_req_body", types::DataType::STRING, types::PatternType::STRUCTURED,
    "Request body in JSON format"},
    {"http_resp_headers", types::DataType::STRING, types::PatternType::STRUCTURED,
    "Response headers in JSON format"},
    {"http_resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM,
    "HTTP response status code"},
    {"http_resp_message", types::DataType::STRING, types::PatternType::STRUCTURED,
    "HTTP response status text (e.g. OK, Not Found, ...)"},
    {"http_resp_body", types::DataType::STRING, types::PatternType::STRUCTURED,
    "Response body in JSON format"},
    // TODO(yzhao): Rename this to latency_ns and consolidate into canonical_types.h.
    {"http_resp_latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
    "Request-response latency in nanoseconds"},
#ifndef NDEBUG
        {"px_info_", types::DataType::STRING, types::PatternType::GENERAL,
                  "Pixie messages regarding the record (e.g. warnings)"},
#endif
};
// clang-format on

enum class HTTPContentType {
  kUnknown = 0,
  kJSON = 1,
  // We use gRPC instead of PB to be consistent with the wording used in gRPC.
  kGRPC = 2,
};

constexpr auto kHTTPTable = DataTableSchema("http_events", kHTTPElements);

constexpr int kHTTPTimeIdx = kHTTPTable.ColIndex("time_");
constexpr int kHTTPUPIDIdx = kHTTPTable.ColIndex("upid");
constexpr int kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
constexpr int kHTTPRemotePortIdx = kHTTPTable.ColIndex("remote_port");
constexpr int kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
constexpr int kHTTPMinorVersionIdx = kHTTPTable.ColIndex("http_minor_version");
constexpr int kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
constexpr int kHTTPReqHeadersIdx = kHTTPTable.ColIndex("http_req_headers");
constexpr int kHTTPReqMethodIdx = kHTTPTable.ColIndex("http_req_method");
constexpr int kHTTPReqPathIdx = kHTTPTable.ColIndex("http_req_path");
constexpr int kHTTPReqBodyIdx = kHTTPTable.ColIndex("http_req_body");
constexpr int kHTTPRespHeadersIdx = kHTTPTable.ColIndex("http_resp_headers");
constexpr int kHTTPRespStatusIdx = kHTTPTable.ColIndex("http_resp_status");
constexpr int kHTTPRespMessageIdx = kHTTPTable.ColIndex("http_resp_message");
constexpr int kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");
constexpr int kHTTPLatencyIdx = kHTTPTable.ColIndex("http_resp_latency_ns");

}  // namespace stirling
}  // namespace pl
