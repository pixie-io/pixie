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
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/types.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kWebSocketElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        canonical_data_elements::kEncrypted,
        {"direction", "1 if the client sent the message, 2 if the server sent it",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"opcode", "WebSocket opcode of the message (1 text, 2 binary, 8 close, 9 ping, 10 pong)",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"compressed", "Whether the message is compressed with permessage-deflate",
         types::DataType::BOOLEAN,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL_ENUM},
        {"fragments", "Number of frames the message was sent in",
         types::DataType::INT64,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"payload", "Message payload, truncated",
         types::DataType::STRING,
         types::SemanticType::ST_NONE,
         types::PatternType::GENERAL},
        {"payload_size", "Message payload size (before any truncation)",
         types::DataType::INT64,
         types::SemanticType::ST_BYTES,
         types::PatternType::METRIC_GAUGE},
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

static constexpr auto kWebSocketTable =
    DataTableSchema("websocket_events", "WebSocket messages", kWebSocketElements);
DEFINE_PRINT_TABLE(WebSocket)

constexpr int kWebSocketDirectionIdx = kWebSocketTable.ColIndex("direction");
constexpr int kWebSocketOpcodeIdx = kWebSocketTable.ColIndex("opcode");
constexpr int kWebSocketPayloadIdx = kWebSocketTable.ColIndex("payload");
constexpr int kWebSocketPayloadSizeIdx = kWebSocketTable.ColIndex("payload_size");

}  // namespace stirling
}  // namespace px
