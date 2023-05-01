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

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"
#include "src/stirling/utils/monitor.h"

namespace px {
namespace stirling {
namespace tcp_stats {

constexpr DataElement addr_data_element(const std::string_view name) {
  return DataElement(name, "IP address of the local/remote endpoint.", types::DataType::STRING,
                     types::SemanticType::ST_IP_ADDRESS, types::PatternType::GENERAL);
}

constexpr DataElement port_data_element(const std::string_view name) {
  return DataElement(name, "Port of the local/remote endpoint.", types::DataType::INT64,
                     types::SemanticType::ST_PORT, types::PatternType::GENERAL);
}

// clang-format off
static constexpr DataElement kTCPStatsElements[] = {
      canonical_data_elements::kTime,
      canonical_data_elements::kUPID,
      addr_data_element("local_addr"),
      port_data_element("local_port"),
      addr_data_element("remote_addr"),
      port_data_element("remote_port"),
      {"tx", "The number of bytes sent to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
      {"rx", "The number of bytes received from the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
      {"retransmits", "The number of retransmissions to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
};

// clang-format on

constexpr auto kTCPStatsTable =
    DataTableSchema("tcp_stats_events", "TCP stats. This table contains TCP connection statistics",
                    kTCPStatsElements);

DEFINE_PRINT_TABLE(TCPStats);

constexpr int kTCPTimeIdx = kTCPStatsTable.ColIndex("time_");
constexpr int kTCPUPIDIdx = kTCPStatsTable.ColIndex("upid");
constexpr int kTCPLocalAddrIdx = kTCPStatsTable.ColIndex("local_addr");
constexpr int kTCPLocalPortIdx = kTCPStatsTable.ColIndex("local_port");
constexpr int kTCPRemoteAddrIdx = kTCPStatsTable.ColIndex("remote_addr");
constexpr int kTCPRemotePortIdx = kTCPStatsTable.ColIndex("remote_port");
constexpr int kTCPBytesSentIdx = kTCPStatsTable.ColIndex("tx");
constexpr int kTCPBytesReceivedIdx = kTCPStatsTable.ColIndex("rx");
constexpr int kTCPRetransmitsIdx = kTCPStatsTable.ColIndex("retransmits");

}  // namespace tcp_stats
}  // namespace stirling
}  // namespace px
