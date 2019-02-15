#pragma once

#include <string>
#include <vector>

#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

class ProcStatConnector : public SourceConnector {
 public:
  ProcStatConnector() = delete;
  explicit ProcStatConnector(const std::string& source_name,
                             const std::vector<InfoClassElement> elements)
      : SourceConnector(SourceType::kFile, source_name, elements) {}
  virtual ~ProcStatConnector() = default;

 protected:
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

}  // namespace datacollector
}  // namespace pl
