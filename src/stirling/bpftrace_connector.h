#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

#ifndef __linux__
namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(CPUStatBPFTraceConnector);

}  // namespace stirling
}  // namespace pl
#else

#include "third_party/bpftrace/src/bpforc.h"
#include "third_party/bpftrace/src/bpftrace.h"

namespace pl {
namespace stirling {

/**
 * @brief Bbpftrace connector
 *
 */
class BPFTraceConnector : public SourceConnector {
 public:
  BPFTraceConnector() = delete;
  virtual ~BPFTraceConnector() = default;

 protected:
  explicit BPFTraceConnector(const std::string& source_name, const DataElements& elements,
                             const char* script, const std::vector<std::string> params);

  Status InitImpl() override;

  RawDataBuf GetDataImpl() override;

  Status StopImpl() override;

  /**
   * @brief If recording nsecs in your bt file, this function can be used to fine the offset for
   * convert the result into realtime.
   */
  uint64_t ClockRealTimeOffset();

 private:
  bpftrace::BPFtrace bpftrace_;
  std::unique_ptr<bpftrace::BpfOrc> bpforc_;
  std::vector<uint8_t> data_buf_;

  uint64_t real_time_offset_;

  // This is the script that will run with this Bpftrace Connector.
  const char* script_;
  std::vector<std::string> params_;
};

class CPUStatBPFTraceConnector : public BPFTraceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;

  static constexpr char kName[] = "bpftrace_cpu_stats";

  inline static const DataElements kElements = {DataElement("_time", DataType::TIME64NS),
                                                DataElement("cpustat_user", DataType::INT64),
                                                DataElement("cpustat_nice", DataType::INT64),
                                                DataElement("cpustat_system", DataType::INT64),
                                                DataElement("cpustat_idle", DataType::INT64),
                                                DataElement("cpustat_iowait", DataType::INT64),
                                                DataElement("cpustat_irq", DataType::INT64),
                                                DataElement("cpustat_softirq", DataType::INT64)};

  inline static const std::chrono::milliseconds kDefaultSamplingPeriod{10};
  inline static const std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new CPUStatBPFTraceConnector(name, cpu_id_));
  }

 protected:
  CPUStatBPFTraceConnector(const std::string& name, uint64_t cpu_id)
      : BPFTraceConnector(name, kElements, kCPUStatBTScript,
                          std::vector<std::string>({std::to_string(cpu_id)})) {}

 private:
  static constexpr char kCPUStatBTScript[] =
#include "cpustat.bt"
      ;  // NOLINT

  // TODO(oazizi): Make this controllable through Create.
  static constexpr uint64_t cpu_id_ = 0;
};

}  // namespace stirling
}  // namespace pl

#endif
