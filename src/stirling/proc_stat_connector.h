#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

class ProcStatConnector : public SourceConnector {
 public:
  // clang-format off
  static constexpr DataElement kElements[] = {
          {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
          {"system_percent", types::DataType::FLOAT64, types::PatternType::METRIC_GAUGE},
          {"user_percent", types::DataType::FLOAT64, types::PatternType::METRIC_GAUGE},
          {"idle_percent", types::DataType::FLOAT64, types::PatternType::METRIC_GAUGE}
  };
  // clang-format on
  static constexpr auto kTable = DataTableSchema("proc_stat", kElements);

  static constexpr DataTableSchema kTablesArray[] = {kTable};
  static constexpr auto kTables = ArrayView<DataTableSchema>(kTablesArray);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  ProcStatConnector() = delete;
  ~ProcStatConnector() override = default;
  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new ProcStatConnector(name));
  }

 protected:
  explicit ProcStatConnector(std::string_view name)
      : SourceConnector(name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod) {}
  Status InitImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;
  Status StopImpl() override { return Status::OK(); }

  /**
   * @brief Read /proc/stat and parse the cpu usage stats into a vector of strings
   *
   * @return std::vector<std::string>
   */
  virtual std::vector<std::string> GetProcParams();

  /**
   * @brief Populate a struct of data values. The values generated should adhere to
   * the elements_ types.
   *
   * @param parsed cpu stat vector of strings from /proc/stat
   * @return Status
   */
  Status GetProcStat(const std::vector<std::string>& parsed_str);

  /**
   * @brief Prevent the compiler from padding the cpu_usage_record struct. For every
   * GetDataImpl call, we populate a struct of CpuUsage, cast it as a uint8_t*
   * and create a RawDataBuf.
   *
   */
  struct __attribute__((__packed__)) CPUUsage {
    uint64_t timestamp;
    double system_percent, user_percent, idle_percent;
  };

  struct CPUStat {
    uint64_t total, system, user, idle;
  };

  CPUUsage cpu_usage_ = {0, 0.0, 0.0, 0.0};
  CPUStat prev_cpu_usage_ = {0, 0, 0, 0};
  const std::string kProcStatFileName = absl::StrCat(sysconfig_.proc_path(), "/stat");
  const int kUserIdx = 1;
  const int kIdleIdx = 4;
  const int kIOWaitIdx = 5;
  const int kNumCPUStatFields = 10;
};

/**
 * @brief Fake proc stat connector used for testing. It generates data in the
 * same format as we expect in /proc/stat on linux systems but with fake data.
 *
 */
class FakeProcStatConnector : public ProcStatConnector {
 public:
  FakeProcStatConnector() = delete;
  ~FakeProcStatConnector() override = default;

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new FakeProcStatConnector(name));
  }

 protected:
  explicit FakeProcStatConnector(std::string_view name) : ProcStatConnector(name) {}

  Status InitImpl() override;

  std::vector<std::string> GetProcParams() override {
    std::string stats = "cpu  ";
    std::vector<std::string> parsed_str;
    for (int i = 0; i < kNumCPUStatFields; ++i) {
      stats += std::to_string(fake_stat_ + i) + " ";
    }
    fake_stat_++;
    parsed_str = absl::StrSplit(stats, ' ', absl::SkipWhitespace());
    return parsed_str;
  }

 private:
  int fake_stat_ = 0;
};

}  // namespace stirling
}  // namespace pl
