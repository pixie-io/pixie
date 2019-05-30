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
  static constexpr SourceType kSourceType = SourceType::kFile;

  static constexpr DataElement kCPUElements[] = {
      {"time_", types::DataType::TIME64NS},      {"qos", types::DataType::STRING},
      {"pod", types::DataType::STRING},          {"container", types::DataType::STRING},
      {"process_name", types::DataType::STRING}, {"pid", types::DataType::INT64},
      {"major_faults", types::DataType::INT64},  {"minor_faults", types::DataType::INT64},
      {"cpu_utime_ns", types::DataType::INT64},  {"cpu_ktime_ns", types::DataType::INT64},
      {"num_threads", types::DataType::INT64},   {"vsize_bytes", types::DataType::INT64},
      {"rss_bytes", types::DataType::INT64},     {"rchar_bytes", types::DataType::INT64},
      {"wchar_bytes", types::DataType::INT64},   {"read_bytes", types::DataType::INT64},
      {"write_bytes", types::DataType::INT64},
  };
  static constexpr auto kCPUTable = DataTableSchema("cgroup_cpu_stats", kCPUElements);

  static constexpr DataElement kNetworkElements[] = {
      {"time_", types::DataType::TIME64NS},  {"pod", types::DataType::STRING},
      {"rx_bytes", types::DataType::INT64},  {"rx_packets", types::DataType::INT64},
      {"rx_errors", types::DataType::INT64}, {"rx_drops", types::DataType::INT64},
      {"tx_bytes", types::DataType::INT64},  {"tx_packets", types::DataType::INT64},
      {"tx_errors", types::DataType::INT64}, {"tx_drops", types::DataType::INT64},
  };
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

  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

 protected:
  explicit CGroupStatsConnector(std::string_view source_name)
      : SourceConnector(kSourceType, source_name, kTables, kDefaultSamplingPeriod,
                        kDefaultPushPeriod) {
    auto sysconfig = common::SystemConfig::Create();
    // TODO(zasgar): Make proc/sys paths configurable.
    cgroup_mgr_ = CGroupManager::Create(*sysconfig, "/proc", "/sys/fs");
  }

 private:
  void TransferCGroupStatsTable(types::ColumnWrapperRecordBatch* record_batch);
  void TransferNetStatsTable(types::ColumnWrapperRecordBatch* record_batch);

  std::unique_ptr<CGroupManager> cgroup_mgr_;
};

}  // namespace stirling
}  // namespace pl

#endif
