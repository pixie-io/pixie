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
  static constexpr char kName[] = "cgroup_stats";
  inline static const std::vector<DataTableSchema> kElements = {DataTableSchema(
      kName,
      {DataElement("time_", types::DataType::TIME64NS), DataElement("qos", types::DataType::STRING),
       DataElement("pod", types::DataType::STRING),
       DataElement("container", types::DataType::STRING),
       DataElement("process_name", types::DataType::STRING),
       DataElement("pid", types::DataType::INT64),
       DataElement("major_faults", types::DataType::INT64),
       DataElement("minor_faults", types::DataType::INT64),
       DataElement("utime_ns", types::DataType::INT64),
       DataElement("ktime_ns", types::DataType::INT64),
       DataElement("num_threads", types::DataType::INT64),
       DataElement("vsize_bytes", types::DataType::INT64),
       DataElement("rss_bytes", types::DataType::INT64),
       DataElement("rchar_bytes", types::DataType::INT64),
       DataElement("wchar_bytes", types::DataType::INT64),
       DataElement("read_bytes", types::DataType::INT64),
       DataElement("write_bytes", types::DataType::INT64)})};

  CGroupStatsConnector() = delete;
  ~CGroupStatsConnector() override = default;

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{1000};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new CGroupStatsConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

 protected:
  explicit CGroupStatsConnector(std::string source_name)
      : SourceConnector(kSourceType, std::move(source_name), kElements, kDefaultSamplingPeriod,
                        kDefaultPushPeriod) {
    auto sysconfig = common::SystemConfig::Create();
    // TODO(zasgar): Make proc/sys paths configurable.
    cgroup_mgr_ = CGroupManager::Create(*sysconfig, "/proc", "/sys/fs");
  }

 private:
  std::unique_ptr<CGroupManager> cgroup_mgr_;
};

}  // namespace stirling
}  // namespace pl

#endif
