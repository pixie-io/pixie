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

#include "src/stirling/core/output.h"

namespace px {
namespace stirling {

// clang-format off
constexpr DataElement kHTTP2MessagesElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"stream_id", "HTTP2 message stream ID",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"headers", "HTTP2 message headers in JSON format",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"body", "HTTP2 message body in JSON format",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::STRUCTURED},
        {"body_size", "HTTP2 message body size (before any truncation)",
         types::DataType::INT64,
         types::SemanticType::ST_BYTES,
         types::PatternType::METRIC_GAUGE},
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

constexpr auto kHTTP2MessagesTable =
    DataTableSchema("http2_messages.beta", "HTTP2 messages events", kHTTPMessagesElements);
DEFINE_PRINT_TABLE(HTTP2Messages)

constexpr int kHTTP2MessagesTimeIdx = kHTTP2MessagesTable.ColIndex("time_");
constexpr int kHTTP2MessagesUPIDIdx = kHTTP2MessagesTable.ColIndex("upid");
constexpr int kHTTP2MessagesRemoteAddrIdx = kHTTP2MessagesTable.ColIndex("remote_addr");
constexpr int kHTTP2MessagesRemotePortIdx = kHTTP2MessagesTable.ColIndex("remote_port");
constexpr int kHTTP2MessagesTraceRoleIdx = kHTTP2MessagesTable.ColIndex("trace_role");
constexpr int kHTTP2MessagesStreamIDIdx = kHTTP2MessagesTable.ColIndex("stream_id");
constexpr int kHTTP2MessagesHeadersIdx = kHTTP2MessagesTable.ColIndex("headers");
constexpr int kHTTP2MessagesBodyIdx = kHTTP2MessagesTable.ColIndex("body");
constexpr int kHTTP2MessagesBodySizeIdx = kHTTP2MessagesTable.ColIndex("body_size");

}  // namespace stirling
}  // namespace px
