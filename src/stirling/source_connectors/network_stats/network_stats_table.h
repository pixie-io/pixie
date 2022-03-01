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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

// clang-format off
constexpr DataElement kNetworkStatsElements[] = {
        canonical_data_elements::kTime,
        {"pod_id", "The ID of the pod",
         types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
        {"rx_bytes", "Received network traffic in bytes of the pod",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"rx_packets", "Number of received network packets of the pod",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"rx_errors", "Number of network receive errors of the pod",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"rx_drops", "Number of dropped network packets being received of the pod",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"tx_bytes", "Transmitted network traffic of the pod",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"tx_packets", "Number of transmitted network packets of the pod",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"tx_errors", "Number of network transmit errors of the pod",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"tx_drops", "Number of dropped network packets being transmitted of the pod",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
};

constexpr DataTableSchema kNetworkStatsTable(
    "network_stats",
    "Network-layer RX/TX stats, grouped by pod. This table contains aggregate statistics "
    "measured at the network device interface. For connection-level information, including the "
    "remote endpoints with which a pod is communicating, see the Connection-Level Stats "
    "(conn_stats) table.",
    kNetworkStatsElements
);
// clang-format on
DEFINE_PRINT_TABLE(NetworkStats);

}  // namespace stirling
}  // namespace px
