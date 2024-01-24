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

namespace px {
namespace stirling {

// clang-format off
constexpr DataElement kNATSElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kLocalAddr,
        canonical_data_elements::kLocalPort,
        canonical_data_elements::kTraceRole,
        {"cmd", "The name of the command.",
         types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
        // For PUB, MSG commands, the parameters and the paylod are included in the 'body' as
        // subfields as well.
        {"body", "Option name and value pairs, formatted as JSON.",
         types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::STRUCTURED},
        {"resp", "The response to the command. One of OK & ERR",
         types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

constexpr DataTableSchema kNATSTable("nats_events.beta", "NATS messages.", kNATSElements);
DEFINE_PRINT_TABLE(NATS)

namespace nats_idx {

constexpr int kTime = kNATSTable.ColIndex("time_");
constexpr int kUPID = kNATSTable.ColIndex("upid");
constexpr int kRemoteAddr = kNATSTable.ColIndex("remote_addr");
constexpr int kRemotePort = kNATSTable.ColIndex("remote_port");
constexpr int kLocalAddr = kNATSTable.ColIndex("local_addr");
constexpr int kLocalPort = kNATSTable.ColIndex("local_port");
constexpr int kRemoteRole = kNATSTable.ColIndex("trace_role");
constexpr int kCMD = kNATSTable.ColIndex("cmd");
constexpr int kOptions = kNATSTable.ColIndex("body");
constexpr int kResp = kNATSTable.ColIndex("resp");
#ifndef NDEBUG
constexpr int kPxInfo = kNATSTable.ColIndex("px_info_");
#endif

}  // namespace nats_idx

}  // namespace stirling
}  // namespace px
