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

namespace pl {
namespace stirling {

class SystemStatsConnector : public SourceConnector {
 public:
  // clang-format off
  static constexpr DataElement kProcessStatsElements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
      {"upid", types::DataType::UINT128, types::PatternType::GENERAL},
      {"major_faults", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"minor_faults", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"cpu_utime_ns", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"cpu_ktime_ns", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"num_threads", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"vsize_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"rss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"rchar_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"wchar_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"read_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"write_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
  };
  // clang-format on
  static constexpr auto kProcessStatsTable =
      DataTableSchema("process_stats", kProcessStatsElements);
  // TODO(oazizi): Enable version below, once rest of the agent supports tabletization.
  //               Can't enable yet because it would result in time-scrambling.
  //  static constexpr std::string_view kProcessStatsTabletizationKey = "upid";
  //  static constexpr auto kProcessStatsTable =
  //      DataTableSchema("process_stats", kProcessStatsElements, kProcessStatsTabletizationKey);

  // clang-format off
  static constexpr DataElement kNetworkStatsElements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
      {"pod_id", types::DataType::STRING, types::PatternType::GENERAL},
      {"rx_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"rx_packets", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"rx_errors", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"rx_drops", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"tx_bytes", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"tx_packets", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"tx_errors", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"tx_drops", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
  };
  // clang-format on
  static constexpr auto kNetworkStatsTable =
      DataTableSchema("network_stats", kNetworkStatsElements);

  static constexpr auto kTables = MakeArray(kProcessStatsTable, kNetworkStatsTable);

  SystemStatsConnector() = delete;
  ~SystemStatsConnector() override = default;

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{1000};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SystemStatsConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit SystemStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod) {
    const auto& sysconfig = system::Config::GetInstance();
    proc_parser_ = std::make_unique<system::ProcParser>(sysconfig);
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
