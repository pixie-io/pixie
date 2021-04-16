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
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

// clang-format off
static constexpr DataElement kProcessStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        {"major_faults", "Number of major page faults",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"minor_faults", "Number of minor page faults",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"cpu_utime_ns", "Time spent on user space by the process",
         types::DataType::INT64, types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_COUNTER},
        {"cpu_ktime_ns", "Time spent on kernel by the process",
         types::DataType::INT64, types::SemanticType::ST_DURATION_NS,
         types::PatternType::METRIC_COUNTER},
        {"num_threads", "Number of threads of the process",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
        {"vsize_bytes", "Virtual memory size in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_GAUGE},
        {"rss_bytes", "Resident memory size in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_GAUGE},
        {"rchar_bytes", "IO reads in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"wchar_bytes", "IO writes in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"read_bytes", "IO reads actually go to storage layer in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"write_bytes", "IO writes actually go to storage layer in bytes of the process",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
};

constexpr DataTableSchema kProcessStatsTable(
    "process_stats",
    "CPU, memory and IO stats for all K8s processes in your cluster.",
    kProcessStatsElements,
    std::chrono::milliseconds{1000},
    std::chrono::milliseconds{1000}
);
// clang-format on

// TODO(oazizi): Enable version below, once rest of the agent supports tabletization.
//               Can't enable yet because it would result in time-scrambling.
//  static constexpr std::string_view kProcessStatsTabletizationKey = "upid";
//  static constexpr auto kProcessStatsTable =
//      DataTableSchema("process_stats", "CPU, memory and IO metrics for processes",
//      kProcessStatsElements, kProcessStatsTabletizationKey);

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
    kNetworkStatsElements,
    std::chrono::milliseconds{1000},
    std::chrono::milliseconds{1000}
);
// clang-format on

}  // namespace stirling
}  // namespace px
