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
constexpr DataElement kConnStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        canonical_data_elements::kTraceRole,
        {"addr_family", "The socket address family of the connection.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL_ENUM,
         &kSockAddrFamilyDecoder},
        {"protocol", "The protocol of the traffic on the connections.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL_ENUM,
         &kTrafficProtocolDecoder},
        {"ssl", "Was SSL traffic detected on this connection.",
         types::DataType::BOOLEAN, types::SemanticType::ST_NONE, types::PatternType::GENERAL_ENUM},
        {"conn_open", "The number of connections opened since the beginning of tracing.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"conn_close", "The number of connections closed since the beginning of tracing.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"conn_active", "The number of active connections",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
        {"bytes_sent", "The number of bytes sent to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"bytes_recv", "The number of bytes received from the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
#ifndef NDEBUG
        canonical_data_elements::kPXInfo,
#endif
};
// clang-format on

constexpr DataTableSchema kConnStatsTable(
    "conn_stats",
    "Connection-level stats. This table contains statistics on the communications made between "
    "client-server pairs. For network-level information such as RX/TX errors and drops, see the "
    "Network-Layer Stats (network_stats) table.",
    kConnStatsElements);
DEFINE_PRINT_TABLE(ConnStats)

namespace conn_stats_idx {

constexpr int kTime = kConnStatsTable.ColIndex("time_");
constexpr int kUPID = kConnStatsTable.ColIndex("upid");
constexpr int kRemoteAddr = kConnStatsTable.ColIndex("remote_addr");
constexpr int kRemotePort = kConnStatsTable.ColIndex("remote_port");
constexpr int kAddrFamily = kConnStatsTable.ColIndex("addr_family");
constexpr int kProtocol = kConnStatsTable.ColIndex("protocol");
constexpr int kRole = kConnStatsTable.ColIndex("trace_role");
constexpr int kSSL = kConnStatsTable.ColIndex("ssl");
constexpr int kConnOpen = kConnStatsTable.ColIndex("conn_open");
constexpr int kConnClose = kConnStatsTable.ColIndex("conn_close");
constexpr int kConnActive = kConnStatsTable.ColIndex("conn_active");
constexpr int kBytesSent = kConnStatsTable.ColIndex("bytes_sent");
constexpr int kBytesRecv = kConnStatsTable.ColIndex("bytes_recv");
#ifndef NDEBUG
constexpr int kPxInfo = kConnStatsTable.ColIndex("px_info_");
#endif

}  // namespace conn_stats_idx

}  // namespace stirling
}  // namespace px
