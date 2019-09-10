#pragma once

namespace pl {
namespace stirling {

// clang-format off
constexpr DataElement kHTTPElements[] = {
        {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
        {"upid", types::DataType::UINT128, types::PatternType::GENERAL},
        // TODO(PL-519): Eventually, use uint128 to represent IP addresses, as will be resolved in
        // the Jira issue.
        {"remote_addr", types::DataType::STRING, types::PatternType::GENERAL},
        {"remote_port", types::DataType::INT64, types::PatternType::GENERAL},
        {"http_major_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
        {"http_minor_version", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
        {"http_content_type", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
        {"http_req_headers", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"http_req_method", types::DataType::STRING, types::PatternType::GENERAL_ENUM},
        {"http_req_path", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"http_req_body", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"http_resp_headers", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"http_resp_status", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
        {"http_resp_message", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"http_resp_body", types::DataType::STRING, types::PatternType::STRUCTURED},
        {"http_resp_latency_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE}
};
// clang-format on

constexpr auto kHTTPTable = DataTableSchema("http_events", kHTTPElements);

constexpr int kHTTPTimeIdx = kHTTPTable.ColIndex("time_");
constexpr int kHTTPUPIDIdx = kHTTPTable.ColIndex("upid");
constexpr int kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
constexpr int kHTTPRemotePortIdx = kHTTPTable.ColIndex("remote_port");
constexpr int kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
constexpr int kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
constexpr int kHTTPReqHeadersIdx = kHTTPTable.ColIndex("http_req_headers");
constexpr int kHTTPReqMethodIdx = kHTTPTable.ColIndex("http_req_method");
constexpr int kHTTPReqPathIdx = kHTTPTable.ColIndex("http_req_path");
constexpr int kHTTPReqBodyIdx = kHTTPTable.ColIndex("http_req_body");
constexpr int kHTTPRespHeadersIdx = kHTTPTable.ColIndex("http_resp_headers");
constexpr int kHTTPRespStatusIdx = kHTTPTable.ColIndex("http_resp_status");
constexpr int kHTTPRespMessageIdx = kHTTPTable.ColIndex("http_resp_message");
constexpr int kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");

}  // namespace stirling
}  // namespace pl
