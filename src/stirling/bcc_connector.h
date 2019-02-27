#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

class BCCConnector : public SourceConnector {
 public:
  static constexpr SourceType source_type = SourceType::kEBPF;
  BCCConnector() = delete;
  virtual ~BCCConnector() = default;

 protected:
  explicit BCCConnector(const std::string& source_name, const DataElements& elements,
                        const std::string& kernel_event, const std::string& fn_name,
                        const std::string& bpf_program)
      : SourceConnector(source_type, source_name, elements),
        kernel_event_(kernel_event),
        fn_name_(fn_name),
        bpf_program_(bpf_program) {}
  Status InitImpl() override {
    // TODO(kgandhi): Launch the EBPF program.
    return Status::OK();
  }

  // TODO(kgandhi): Get data records from EBPF program. Placeholder for now.
  RawDataBuf GetDataImpl() override {
    uint64_t num_records = 1;
    return RawDataBuf(num_records, data_buf_.data());
  };

  // TODO(kgandhi): Stop the running EBPF program.
  Status StopImpl() override { return Status::OK(); }

  const std::string& kernel_event() { return kernel_event_; }
  const std::string& fn_name() { return fn_name_; }
  const std::string& bpf_program() { return bpf_program_; }

 private:
  std::string kernel_event_, fn_name_, bpf_program_;
  std::vector<uint8_t> data_buf_;
};

class BCCCPUMetricsConnector : public BCCConnector {
 public:
  // TODO(kgandhi): Remove next line once this SourceConnector is functional.
  static constexpr bool kAvailable = false;

  static constexpr char kName[] = "bcc_cpu_stats";

  inline static const DataElements kElements = {DataElement("_time", DataType::TIME64NS),
                                                DataElement("cpu_id", DataType::INT64),
                                                DataElement("cpu_percentage", DataType::FLOAT64)};

  virtual ~BCCCPUMetricsConnector() = default;

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    // EBPF CPU Data Source.
    // TODO(kgandhi): Coming in a future diff. Adding a bpf program to the end of an object file
    // currently only works on linux builds. We plan to add ifdefs around that to prevent breaking
    // the builds on other platforms.
    char prog = 0;
    // There will be two extern chars pointing to locations in the obj file (marking
    // start and end).
    char* bpf_prog_ptr = &prog;
    int bpf_prog_len = 0;
    const std::string bpf_program = std::string(bpf_prog_ptr, bpf_prog_len);

    return std::unique_ptr<SourceConnector>(new BCCCPUMetricsConnector(
        name, kElements, "finish_task_switch", "task_switch_event", bpf_program));
  }

 protected:
  explicit BCCCPUMetricsConnector(const std::string& source_name, const DataElements& elements,
                                  const std::string& kernel_event, const std::string& fn_name,
                                  const std::string& bpf_program)
      : BCCConnector(source_name, elements, kernel_event, fn_name, bpf_program) {}
};

}  // namespace stirling
}  // namespace pl
