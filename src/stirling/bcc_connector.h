#pragma once

#ifndef __linux__

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(PIDCPUUseBCCConnector);

}  // namespace stirling
}  // namespace pl
#else

#include <bcc/BPF.h>
#include <linux/perf_event.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf/pidruntime.h"
#include "src/stirling/source_connector.h"

OBJ_STRVIEW(pidruntime_bcc_script, _binary_src_stirling_bcc_bpf_pidruntime_c);

namespace pl {
namespace stirling {

class BCCConnector : public SourceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;
  BCCConnector() = delete;
  ~BCCConnector() override = default;

 protected:
  explicit BCCConnector(std::string source_name, const DataElements& elements,
                        std::chrono::milliseconds default_sampling_period,
                        std::chrono::milliseconds default_push_period,
                        const std::string_view bpf_program)
      : SourceConnector(kSourceType, std::move(source_name), elements, default_sampling_period,
                        default_push_period),
        bpf_program_(bpf_program) {}

 private:
  std::string_view bpf_program_;
};

class PIDCPUUseBCCConnector : public BCCConnector {
 public:
  inline static const std::string_view kBCCScript = pidruntime_bcc_script;
  static constexpr SourceType kSourceType = SourceType::kEBPF;
  static constexpr char kName[] = "bcc_pid_cpu_usage";
  inline static const DataElements kElements = {DataElement("time_", types::DataType::TIME64NS),
                                                DataElement("pid", types::DataType::INT64),
                                                DataElement("runtime_ns", types::DataType::INT64),
                                                DataElement("cmd", types::DataType::STRING)};

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new PIDCPUUseBCCConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) override;

 protected:
  explicit PIDCPUUseBCCConnector(std::string name)
      : BCCConnector(name, kElements, kDefaultSamplingPeriod, kDefaultPushPeriod, kBCCScript),
        event_type_(perf_type_id::PERF_TYPE_SOFTWARE),
        event_config_(perf_sw_ids::PERF_COUNT_SW_CPU_CLOCK) {}

 private:
  static constexpr char kFunctionName[] = "trace_pid_runtime";

  uint32_t event_type_;
  uint32_t event_config_;
  std::map<uint16_t, uint64_t> prev_run_time_map_;
  std::vector<std::pair<uint16_t, pl_stirling_bcc_pidruntime_val> > table_;
  static constexpr uint64_t kSamplingFreq = 99;  // Freq. (in Hz) at which to trigger bpf func.
  ebpf::BPF bpf_;
};

}  // namespace stirling
}  // namespace pl

#endif
