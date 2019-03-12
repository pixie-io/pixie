#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

#ifndef __linux__
namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(CPUStatBPFTraceConnector);
DUMMY_SOURCE_CONNECTOR(PIDCPUUseBPFTraceConnector);

}  // namespace stirling
}  // namespace pl
#else

#include "third_party/bpftrace/src/bpforc.h"
#include "third_party/bpftrace/src/bpftrace.h"

extern char _binary_src_stirling_bt_cpustat_bt_start;
extern char _binary_src_stirling_bt_cpustat_bt_end;
extern char _binary_src_stirling_bt_cpustat_bt_size;

extern char _binary_src_stirling_bt_pidruntime_bt_start;
extern char _binary_src_stirling_bt_pidruntime_bt_end;
extern char _binary_src_stirling_bt_pidruntime_bt_size;

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
                             const std::string& script, const std::vector<std::string> params);

  Status InitImpl() override;

  Status StopImpl() override;

  bpftrace::BPFTraceMap GetBPFMap(const std::string& name) { return bpftrace_.get_map(name); }

 protected:
  /**
   * @brief If recording nsecs in your bt file, this function can be used to find the offset for
   * convert the result into realtime.
   */
  uint64_t ClockRealTimeOffset();

 private:
  // This is the script that will run with this Bpftrace Connector.
  const std::string& script_;
  std::vector<std::string> params_;

  bpftrace::BPFtrace bpftrace_;
  std::unique_ptr<bpftrace::BpfOrc> bpforc_;

  uint64_t real_time_offset_;

  // Init Helper function: calculates monotonic clock to real time clock offset.
  void InitClockRealTimeOffset();
};

class CPUStatBPFTraceConnector : public BPFTraceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;

  static constexpr char kName[] = "bpftrace_cpu_stats";

  inline static const DataElements kElements = {DataElement("time_", DataType::TIME64NS),
                                                DataElement("cpustat_user", DataType::INT64),
                                                DataElement("cpustat_nice", DataType::INT64),
                                                DataElement("cpustat_system", DataType::INT64),
                                                DataElement("cpustat_idle", DataType::INT64),
                                                DataElement("cpustat_iowait", DataType::INT64),
                                                DataElement("cpustat_irq", DataType::INT64),
                                                DataElement("cpustat_softirq", DataType::INT64)};

  inline static const std::chrono::milliseconds kDefaultSamplingPeriod{100};
  inline static const std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new CPUStatBPFTraceConnector(name, cpu_id_));
  }

  RawDataBuf GetDataImpl() override;

 protected:
  explicit CPUStatBPFTraceConnector(const std::string& name, uint64_t cpu_id);

 private:
  inline static const std::string kCPUStatBTScript = std::string(
      &_binary_src_stirling_bt_cpustat_bt_start,
      &_binary_src_stirling_bt_cpustat_bt_end - &_binary_src_stirling_bt_cpustat_bt_start);

  // TODO(oazizi): Make this controllable through Create.
  static constexpr uint64_t cpu_id_ = 0;

  std::vector<uint64_t> data_buf_;
};

class PIDCPUUseBPFTraceConnector : public BPFTraceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;

  static constexpr char kName[] = "bpftrace_pid_cpu_usage";

  inline static const DataElements kElements = {
      DataElement("time_", DataType::TIME64NS), DataElement("pid", DataType::INT64),
      DataElement("runtime_ns", DataType::INT64), DataElement("cmd", DataType::STRING)};

  inline static const std::chrono::milliseconds kDefaultSamplingPeriod{1000};
  inline static const std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new PIDCPUUseBPFTraceConnector(name));
  }

  RawDataBuf GetDataImpl() override;

 protected:
  explicit PIDCPUUseBPFTraceConnector(const std::string& name);

 private:
  const std::string kBTScript = std::string(
      &_binary_src_stirling_bt_pidruntime_bt_start,
      &_binary_src_stirling_bt_pidruntime_bt_end - &_binary_src_stirling_bt_pidruntime_bt_start);

  std::vector<uint64_t> data_buf_;

  bpftrace::BPFTraceMap last_result_times_;

  // This is a member variable to avoid copying the strings.
  bpftrace::BPFTraceMap pid_name_pairs_;

  bpftrace::BPFTraceMap::iterator BPFTraceMapSearch(const bpftrace::BPFTraceMap& vector,
                                                    bpftrace::BPFTraceMap::iterator it,
                                                    uint64_t search_key);
};

}  // namespace stirling
}  // namespace pl

#endif
