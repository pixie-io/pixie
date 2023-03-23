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

#include <prometheus/counter.h>

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"
#include "src/stirling/source_connectors/tcp_stats/canonical_types.h"
#include "src/stirling/utils/monitor.h"

namespace px {
namespace stirling {
namespace tcp_stats {

// clang-format off
static constexpr DataElement kTCPStatsElements[] = {
      canonical_data_elements::kTime,
      canonical_data_elements::kUPID,
      canonical_data_elements_net::kLocalAddr,
      canonical_data_elements_net::kLocalPort,
      canonical_data_elements_net::kRemoteAddr,
      canonical_data_elements_net::kRemotePort,
      {"tx", "The number of bytes sent to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
      {"rx", "The number of retransmissions to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
      {"retransmits", "The number of retransmissions to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
};

// clang-format on

constexpr auto kTCPStatsTable =
    DataTableSchema("tcp_stats_events", "TCP stats. This table contains TCP connection statistics",
                    kTCPStatsElements);

DEFINE_PRINT_TABLE(TCPStats);

constexpr int kTcpTimeIdx = kTCPStatsTable.ColIndex("time_");
constexpr int kTcpUPIDIdx = kTCPStatsTable.ColIndex("upid");
constexpr int kTcpLocalAddrIdx = kTCPStatsTable.ColIndex("local_addr");
constexpr int kTcpLocalPortIdx = kTCPStatsTable.ColIndex("local_port");
constexpr int kTcpRemoteAddrIdx = kTCPStatsTable.ColIndex("remote_addr");
constexpr int kTcpRemotePortIdx = kTCPStatsTable.ColIndex("remote_port");
constexpr int kTcpBytesSentIdx = kTCPStatsTable.ColIndex("tx");
constexpr int kTcpBytesReceivedIdx = kTCPStatsTable.ColIndex("rx");
constexpr int kTcpRetransmitsIdx = kTCPStatsTable.ColIndex("retransmits");

}  // namespace tcp_stats
}  // namespace stirling
}  // namespace px
