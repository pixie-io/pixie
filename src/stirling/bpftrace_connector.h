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

#include "src/common/obj_tools.h"
#include "third_party/bpftrace/src/bpforc.h"
#include "third_party/bpftrace/src/bpftrace.h"

// The following are string_views into BT files that are included in the binary by the linker.
// The BT files are permanently resident in memory, so the string view is permanent too.
OBJ_STRVIEW(cpustat_bt_script, _binary_src_stirling_bt_cpustat_bt);
OBJ_STRVIEW(pidruntime_bt_script, _binary_src_stirling_bt_pidruntime_bt);

namespace pl {
namespace stirling {

/**
 * @brief Bbpftrace connector
 *
 */
class BPFTraceConnector : public SourceConnector {
 public:
  BPFTraceConnector() = delete;
  ~BPFTraceConnector() override = default;

 protected:
  explicit BPFTraceConnector(const std::string& source_name, const DataElements& elements,
                             std::string_view script, std::vector<std::string> params);

  Status InitImpl() override;

  Status StopImpl() override;

  bpftrace::BPFTraceMap GetBPFMap(const std::string& name) { return bpftrace_.get_map(name); }

 private:
  // This is the script that will run with this Bpftrace Connector.
  std::string_view script_;

  // List of params to the program (like argv).
  std::vector<std::string> params_;

  bpftrace::BPFtrace bpftrace_;
  std::unique_ptr<bpftrace::BpfOrc> bpforc_;
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

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new CPUStatBPFTraceConnector(name, cpu_id_));
  }

  RawDataBuf GetDataImpl() override;

 protected:
  explicit CPUStatBPFTraceConnector(const std::string& name, uint64_t cpu_id);

 private:
  inline static const std::string_view kBTScript = cpustat_bt_script;

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

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{1000};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new PIDCPUUseBPFTraceConnector(name));
  }

  RawDataBuf GetDataImpl() override;

 protected:
  explicit PIDCPUUseBPFTraceConnector(const std::string& name);

 private:
  inline static const std::string_view kBTScript = pidruntime_bt_script;

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
