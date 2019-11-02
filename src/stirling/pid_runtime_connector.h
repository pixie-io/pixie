#pragma once

#ifndef __linux__

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(PIDRuntimeConnector);

}  // namespace stirling
}  // namespace pl
#else

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/pidruntime.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/common/utils.h"
#include "src/stirling/source_connector.h"

BCC_SRC_STRVIEW(pidruntime_bcc_script, pidruntime);

namespace pl {
namespace stirling {

class PIDRuntimeConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  inline static const std::string_view kBCCScript = pidruntime_bcc_script;

  // clang-format off
  static constexpr DataElement kElements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
      {"pid", types::DataType::INT64, types::PatternType::GENERAL},
      // TODO(chengruizhe): runtime_ns: Will be converted to counter
      {"runtime_ns", types::DataType::INT64, types::PatternType::METRIC_GAUGE},
      {"cmd", types::DataType::STRING, types::PatternType::GENERAL},
  };
  // clang-format on
  static constexpr auto kTable = DataTableSchema("bcc_pid_cpu_usage", kElements);

  static constexpr DataTableSchema kTablesArray[] = {kTable};
  static constexpr auto kTables = ArrayView<DataTableSchema>(kTablesArray);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PIDRuntimeConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit PIDRuntimeConnector(std::string_view name)
      : SourceConnector(name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod),
        bpf_tools::BCCWrapper(kBCCScript) {}

 private:
  static constexpr perf_type_id kEventType = perf_type_id::PERF_TYPE_SOFTWARE;
  static constexpr perf_sw_ids kEventConfig = perf_sw_ids::PERF_COUNT_SW_CPU_CLOCK;
  static constexpr char kFunctionName[] = "trace_pid_runtime";
  static constexpr uint64_t kSamplingFreq = 99;  // Freq. (in Hz) at which to trigger bpf func.

  static constexpr bpf_tools::PerfEventSpec kPerfEventsArray[] = {
      {kEventType, kEventConfig, kFunctionName, 0, kSamplingFreq}};
  static constexpr auto kPerfEvents = ArrayView<bpf_tools::PerfEventSpec>(kPerfEventsArray);

  std::map<uint16_t, uint64_t> prev_run_time_map_;
  std::vector<std::pair<uint16_t, pidruntime_val_t> > table_;
};

}  // namespace stirling
}  // namespace pl

#endif
