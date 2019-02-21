#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

#ifndef __linux__
namespace pl {
namespace stirling {

class CPUStatBPFTraceConnector : public NotImplementedSourceConnector {
 public:
  static constexpr SourceType source_type = SourceType::kUnavailable;
  static std::unique_ptr<SourceConnector> Create() { return nullptr; }
};

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
  explicit BPFTraceConnector(const std::string& source_name,
                             const std::vector<InfoClassElement> elements, const char* script);

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
};

class CPUStatBPFTraceConnector : public BPFTraceConnector {
 public:
  static constexpr SourceType source_type = SourceType::kEBPF;

  static std::unique_ptr<SourceConnector> Create() {
    std::vector<InfoClassElement> elements = {
        InfoClassElement("_time", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_user", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_nice", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_system", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_idle", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_iowait", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_irq", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED),
        InfoClassElement("cpustat_softirq", DataType::INT64,
                         Element_State::Element_State_COLLECTED_AND_SUBSCRIBED)};

    return std::unique_ptr<SourceConnector>(
        new CPUStatBPFTraceConnector("CPU Stat Bpftrace connector", elements));
  }

 protected:
  CPUStatBPFTraceConnector(const std::string& source_name,
                           const std::vector<InfoClassElement> elements)
      : BPFTraceConnector(source_name, elements, kCPUStatBTScript) {}

 private:
  static constexpr char kCPUStatBTScript[] =
#include "cpustat.bt"
      ;  // NOLINT
};

}  // namespace stirling
}  // namespace pl

#endif
