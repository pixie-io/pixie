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

#include "src/common/base/base.h"
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
  explicit BPFTraceConnector(std::string_view source_name,
                             const ConstVectorView<DataTableSchema>& elements,
                             std::chrono::milliseconds default_sampling_period,
                             std::chrono::milliseconds default_push_period, std::string_view script,
                             std::vector<std::string> params);

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

  // clang-format off
  static constexpr DataElement kElements[] = {
      {"time_", types::DataType::TIME64NS},
      {"cpustat_user", types::DataType::INT64},
      {"cpustat_nice", types::DataType::INT64},
      {"cpustat_system", types::DataType::INT64},
      {"cpustat_idle", types::DataType::INT64},
      {"cpustat_iowait", types::DataType::INT64},
      {"cpustat_irq", types::DataType::INT64},
      {"cpustat_softirq", types::DataType::INT64}
  };
  // clang-format on
  static constexpr auto kTable = DataTableSchema("bpftrace_cpu_stats", kElements);
  static constexpr DataTableSchema kTablesArray[] = {kTable};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new CPUStatBPFTraceConnector(name, cpu_id_));
  }

  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

 protected:
  explicit CPUStatBPFTraceConnector(std::string_view name, uint64_t cpu_id);

 private:
  inline static const std::string_view kBTScript = cpustat_bt_script;

  // TODO(oazizi): Make this controllable through Create.
  static constexpr uint64_t cpu_id_ = 0;
};

class PIDCPUUseBPFTraceConnector : public BPFTraceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;

  // clang-format off
  static constexpr DataElement kElements[] = {
      {"time_", types::DataType::TIME64NS},
      {"pid", types::DataType::INT64},
      {"runtime_ns", types::DataType::INT64},
      {"cmd", types::DataType::STRING}
  };
  // clang-format on
  static constexpr auto kTable = DataTableSchema("bpftrace_pid_cpu_usage", kElements);
  static constexpr DataTableSchema kTablesArray[] = {kTable};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{1000};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PIDCPUUseBPFTraceConnector(name));
  }

  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

 protected:
  explicit PIDCPUUseBPFTraceConnector(std::string_view name);

 private:
  inline static const std::string_view kBTScript = pidruntime_bt_script;

  bpftrace::BPFTraceMap last_result_times_;

  bpftrace::BPFTraceMap::iterator BPFTraceMapSearch(const bpftrace::BPFTraceMap& vector,
                                                    bpftrace::BPFTraceMap::iterator it,
                                                    uint64_t search_key);
};

}  // namespace stirling
}  // namespace pl

#endif
