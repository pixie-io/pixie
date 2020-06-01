#pragma once

#include "src/stirling/source_connector.h"

#ifndef __linux__

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(SystemStatsConnector);

}  // namespace stirling
}  // namespace pl
#else

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/canonical_types.h"

namespace pl {
namespace stirling {

class SystemStatsConnector : public SourceConnector {
 public:
  // clang-format off
  static constexpr DataElement kProcessStatsElements[] = {
      canonical_data_elements::kTime,
      canonical_data_elements::kUPID,
      {"major_faults", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of major page faults"},
      {"minor_faults", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of minor page faults"},
      {"cpu_utime_ns", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Time spent on user space by the process"},
      {"cpu_ktime_ns", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Time spent on kernel by the process"},
      {"num_threads", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
      "Number of threads of the process"},
      {"vsize_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
      "Virtual memory size in bytes of the process"},
      {"rss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE,
      "Resident memory size in bytes of the process"},
      {"rchar_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "IO reads in bytes of the process"},
      {"wchar_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "IO writes in bytes of the process"},
      {"read_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "IO reads actually go to storage layer in bytes of the process"},
      {"write_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "IO writes actually go to storage layer in bytes of the process"},
  };
  // clang-format on
  static constexpr auto kProcessStatsTable =
      DataTableSchema("process_stats", kProcessStatsElements, std::chrono::milliseconds{1000},
                      std::chrono::milliseconds{1000});
  // TODO(oazizi): Enable version below, once rest of the agent supports tabletization.
  //               Can't enable yet because it would result in time-scrambling.
  //  static constexpr std::string_view kProcessStatsTabletizationKey = "upid";
  //  static constexpr auto kProcessStatsTable =
  //      DataTableSchema("process_stats", kProcessStatsElements, kProcessStatsTabletizationKey);

  // clang-format off
  static constexpr DataElement kNetworkStatsElements[] = {
      canonical_data_elements::kTime,
      {"pod_id", types::DataType::STRING, types::PatternType::GENERAL,
      "The ID of the pod"},
      {"rx_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Received network traffic in bytes of the pod"},
      {"rx_packets", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of received network packets of the pod"},
      {"rx_errors", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of network receive errors of the pod"},
      {"rx_drops", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of dropped network packets being received of the pod"},
      {"tx_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Transmitted network traffic of the pod"},
      {"tx_packets", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of transmitted network packets of the pod"},
      {"tx_errors", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of network transmit errors of the pod"},
      {"tx_drops", types::DataType::INT64, types::PatternType::METRIC_COUNTER,
      "Number of dropped network packets being transmitted of the pod"},
  };
  // clang-format on
  static constexpr auto kNetworkStatsTable =
      DataTableSchema("network_stats", kNetworkStatsElements, std::chrono::milliseconds{1000},
                      std::chrono::milliseconds{1000});

  static constexpr auto kTables = MakeArray(kProcessStatsTable, kNetworkStatsTable);

  SystemStatsConnector() = delete;
  ~SystemStatsConnector() override = default;

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SystemStatsConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit SystemStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables) {
    proc_parser_ = std::make_unique<system::ProcParser>(sysconfig_);
  }

 private:
  void TransferProcessStatsTable(ConnectorContext* ctx, DataTable* data_table);
  void TransferNetworkStatsTable(ConnectorContext* ctx, DataTable* data_table);

  static Status GetNetworkStatsForPod(const system::ProcParser& proc_parser,
                                      const md::PodInfo& pod_info,
                                      const md::K8sMetadataState& k8s_metadata_state,
                                      system::ProcParser::NetworkStats* stats);

  std::unique_ptr<system::ProcParser> proc_parser_;
};

}  // namespace stirling
}  // namespace pl

#endif
