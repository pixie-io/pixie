#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

class ProcStatConnector : public SourceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kFile;

  static constexpr char kName[] = "proc_stat";

  inline static DataElements kElements = {DataElement("time_", DataType::TIME64NS),
                                          DataElement("system_percent", DataType::FLOAT64),
                                          DataElement("user_percent", DataType::FLOAT64),
                                          DataElement("idle_percent", DataType::FLOAT64)};

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  ProcStatConnector() = delete;
  ~ProcStatConnector() override = default;
  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new ProcStatConnector(name, kElements));
  }

 protected:
  explicit ProcStatConnector(const std::string& name, const DataElements& elements)
      : SourceConnector(kSourceType, name, elements) {}
  Status InitImpl() override;
  RawDataBuf GetDataImpl() override;
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
    uint64_t time_stamp;
    double system_percent, user_percent, idle_percent;
  };

  struct CPUStat {
    uint64_t total, system, user, idle;
  };

  CPUUsage cpu_usage_;
  CPUStat prev_cpu_usage_ = {0, 0, 0, 0};
  uint8_t* data_buf_;
  const char* kProcStatFileName = "/proc/stat";
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

  static constexpr char kName[] = "fake_proc_stat";

  inline static DataElements kElements = {DataElement("time_", DataType::TIME64NS),
                                          DataElement("system_percent", DataType::FLOAT64),
                                          DataElement("user_percent", DataType::FLOAT64),
                                          DataElement("idle_percent", DataType::FLOAT64)};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new FakeProcStatConnector(name, kElements));
  }

 protected:
  explicit FakeProcStatConnector(const std::string& name, const DataElements& elements)
      : ProcStatConnector(name, elements) {}

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
