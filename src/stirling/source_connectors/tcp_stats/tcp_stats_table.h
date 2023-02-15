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

#include "src/stirling/core/output.h"
#include "src/stirling/core/types.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"
#include "src/stirling/source_connectors/tcp_stats/canonical_types.h"
#include "src/stirling/utils/monitor.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kTCPTXElements[] = {
      canonical_data_elements_net::kRemoteAddr,
      {"cmd", "Process command line",
       types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      {"bytes_sent", "The number of bytes sent to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_GAUGE},
};

static constexpr DataElement kTCPRXElements[] = {
      canonical_data_elements_net::kRemoteAddr,
      {"cmd", "Process command line",
       types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      {"bytes_received", "The number of bytes received to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_GAUGE},
};

static constexpr DataElement kTCPRetransElements[] = {
      canonical_data_elements_net::kRemoteAddr,
      {"cmd", "Process command line",
       types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      {"retrans", "The number of retransmissions to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
};

constexpr DataTableSchema kTCPTXStatsTable(
        "tcp_tx_stats",
        "TCP tx stats. This table contains statistics on the number of TCP bytes sent",
        kTCPTXElements
);

constexpr DataTableSchema kTCPRXStatsTable(
        "tcp_rx_stats",
        "TCP rx stats. This table contains statistics on the number of TCP bytes received",
        kTCPRXElements
);

constexpr DataTableSchema kTCPRetransStatsTable(
        "tcp_retrans_stats",
        "TCP retrans stats. This table contains statistics on the number of TCP retransmissions",
        kTCPRetransElements
);

}  // namespace stirling
}  // namespace px
