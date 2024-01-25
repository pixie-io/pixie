/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>

#include "src/stirling/core/output.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"

namespace px {
namespace stirling {

enum class HTTPContentType {
  kUnknown = 0,
  kJSON = 1,
  // We use gRPC instead of PB to be consistent with the wording used in gRPC.
  kGRPC = 2,
};

static const std::map<int64_t, std::string_view> kHTTPContentTypeDecoder =
    px::EnumDefToMap<HTTPContentType>();

// clang-format off
constexpr DataElement kHTTPElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"major_version", "HTTP major version, can be 1 or 2",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"minor_version", "HTTP minor version, HTTP1 uses 1, HTTP2 set this value to 0",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"content_type", "Type of the HTTP payload, can be JSON or protobuf",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM,
         &kHTTPContentTypeDecoder},
        {"req_headers", "Request headers in JSON format",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"req_method", "HTTP request method (e.g. GET, POST, ...)",
         types::DataType::STRING,
         types::SemanticType::ST_HTTP_REQ_METHOD,
         types::PatternType::GENERAL_ENUM},
        {"req_path", "Request path",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"req_body", "Request body in JSON format",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"req_body_size", "Request body size (before any truncation)",
         types::DataType::INT64,
         types::SemanticType::ST_BYTES,
         types::PatternType::METRIC_GAUGE},
        {"resp_headers", "Response headers in JSON format",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"resp_status", "HTTP response status code",
         types::DataType::INT64,
         types::SemanticType::ST_HTTP_RESP_STATUS,
         types::PatternType::GENERAL_ENUM},
        {"resp_message", "HTTP response status text (e.g. OK, Not Found, ...)",
         types::DataType::STRING,
         types::SemanticType::ST_HTTP_RESP_MESSAGE,
         types::PatternType::STRUCTURED},
        {"resp_body", "Response body in JSON format",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"resp_body_size", "Response body size (before any truncation)",
         types::DataType::INT64,
         types::SemanticType::ST_BYTES,
         types::PatternType::METRIC_GAUGE},
        canonical_data_elements::kLatencyNS,
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

constexpr auto kHTTPTable =
    DataTableSchema("http_events", "HTTP request-response pair events", kHTTPElements);
DEFINE_PRINT_TABLE(HTTP)

constexpr int kHTTPTimeIdx = kHTTPTable.ColIndex("time_");
constexpr int kHTTPUPIDIdx = kHTTPTable.ColIndex("upid");
constexpr int kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
constexpr int kHTTPRemotePortIdx = kHTTPTable.ColIndex("remote_port");
constexpr int kHTTPLocalAddrIdx = kHTTPTable.ColIndex("local_addr");
constexpr int kHTTPLocalPortIdx = kHTTPTable.ColIndex("local_port");
constexpr int kHTTPTraceRoleIdx = kHTTPTable.ColIndex("trace_role");
constexpr int kHTTPMajorVersionIdx = kHTTPTable.ColIndex("major_version");
constexpr int kHTTPMinorVersionIdx = kHTTPTable.ColIndex("minor_version");
constexpr int kHTTPContentTypeIdx = kHTTPTable.ColIndex("content_type");
constexpr int kHTTPReqHeadersIdx = kHTTPTable.ColIndex("req_headers");
constexpr int kHTTPReqMethodIdx = kHTTPTable.ColIndex("req_method");
constexpr int kHTTPReqPathIdx = kHTTPTable.ColIndex("req_path");
constexpr int kHTTPReqBodyIdx = kHTTPTable.ColIndex("req_body");
constexpr int kHTTPReqBodySizeIdx = kHTTPTable.ColIndex("req_body_size");
constexpr int kHTTPRespHeadersIdx = kHTTPTable.ColIndex("resp_headers");
constexpr int kHTTPRespStatusIdx = kHTTPTable.ColIndex("resp_status");
constexpr int kHTTPRespMessageIdx = kHTTPTable.ColIndex("resp_message");
constexpr int kHTTPRespBodyIdx = kHTTPTable.ColIndex("resp_body");
constexpr int kHTTPRespBodySizeIdx = kHTTPTable.ColIndex("resp_body_size");
constexpr int kHTTPLatencyIdx = kHTTPTable.ColIndex("latency");

}  // namespace stirling
}  // namespace px
