#pragma once

#include "src/stirling/source_connector.h"

#ifndef __linux__

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(CGroupStatsConnector);

}  // namespace stirling
}  // namespace pl
#else

#include <experimental/filesystem>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

class CGroupStatsConnector : public SourceConnector {
 public:
  // clang-format off
  static constexpr DataElement kCPUElements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
      {"qos", types::DataType::STRING, types::PatternType::GENERAL_ENUM},
      {"pod", types::DataType::STRING, types::PatternType::GENERAL},
      {"container", types::DataType::STRING, types::PatternType::GENERAL},
      {"process_name", types::DataType::STRING, types::PatternType::GENERAL},
      {"pid", types::DataType::INT64, types::PatternType::GENERAL},
      {"major_faults", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"minor_faults", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"cpu_utime_ns", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"cpu_ktime_ns", types::DataType::INT64, types::PatternType::METRIC_COUNTER},
      {"num_threads", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"vsize_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"rss_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"rchar_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"wchar_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"read_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"write_bytes", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
  };
  // clang-format on
  static constexpr auto kCPUTable = DataTableSchema("cgroup_cpu_stats", kCPUElements);

  // clang-format off
  static constexpr DataElement kNetworkElements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
      {"pod", types::DataType::STRING, types::PatternType::GENERAL},
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
  static constexpr auto kNetworkTable = DataTableSchema("cgroup_net_stats", kNetworkElements);

  static constexpr DataTableSchema kTablesArray[] = {kCPUTable, kNetworkTable};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);

  CGroupStatsConnector() = delete;
  ~CGroupStatsConnector() override = default;

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{1000};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new CGroupStatsConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit CGroupStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod) {
    auto sysconfig = common::SystemConfig::GetInstance();
    cgroup_mgr_ = CGroupManager::Create(*sysconfig);
  }

 private:
  void TransferCGroupStatsTable(DataTable* data_table);
  void TransferNetStatsTable(DataTable* data_table);

  std::unique_ptr<CGroupManager> cgroup_mgr_;
};

}  // namespace stirling
}  // namespace pl

#endif
