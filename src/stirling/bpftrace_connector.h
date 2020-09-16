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
#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

namespace pl {
namespace stirling {

/**
 * @brief Bpftrace connector
 *
 */
class BPFTraceConnector : public SourceConnector, public bpf_tools::BPFTraceWrapper {
 public:
  BPFTraceConnector() = delete;
  ~BPFTraceConnector() override = default;

 protected:
  explicit BPFTraceConnector(std::string_view source_name,
                             const ArrayView<DataTableSchema>& table_schemas,
                             const std::string_view script, std::vector<std::string> params);

  Status InitImpl() override;

  Status StopImpl() override;

 private:
  // This is the script that will run with this Bpftrace Connector.
  std::string_view script_;

  // List of params to the program (like argv).
  std::vector<std::string> params_;
};

class CPUStatBPFTraceConnector : public BPFTraceConnector {
 public:
  // clang-format off
  static constexpr DataElement kElements[] = {
      {
        "time_",
        "Timestamp when the data record was collected.",
        types::DataType::TIME64NS,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_user",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_nice",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_system",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_idle",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_iowait",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_irq",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cpustat_softirq",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      }
  };
  // clang-format on
  static constexpr auto kTable =
      DataTableSchema("bpftrace_cpu_stats", kElements, std::chrono::milliseconds{100},
                      std::chrono::milliseconds{1000});
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new CPUStatBPFTraceConnector(name, cpu_id_));
  }

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit CPUStatBPFTraceConnector(std::string_view name, uint64_t cpu_id);

 private:
  // TODO(oazizi): Make this controllable through Create.
  static constexpr uint64_t cpu_id_ = 0;
};

class PIDCPUUseBPFTraceConnector : public BPFTraceConnector {
 public:
  // clang-format off
  static constexpr DataElement kElements[] = {
      {
        "time_",
        "",
        types::DataType::TIME64NS,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "pid",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
      },
      {
        "runtime_ns",
        "",
        types::DataType::INT64,
        types::SemanticType::ST_NONE,
        types::PatternType::METRIC_COUNTER
      },
      {
        "cmd",
        "",
        types::DataType::STRING,
        types::SemanticType::ST_NONE,
        types::PatternType::GENERAL
      }
  };
  // clang-format on
  static constexpr auto kTable =
      DataTableSchema("bpftrace_pid_cpu_usage", kElements, std::chrono::milliseconds{100},
                      std::chrono::milliseconds{1000});
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PIDCPUUseBPFTraceConnector(name));
  }

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit PIDCPUUseBPFTraceConnector(std::string_view name);

 private:
  bpftrace::BPFTraceMap last_result_times_;

  bpftrace::BPFTraceMap::iterator BPFTraceMapSearch(const bpftrace::BPFTraceMap& vector,
                                                    bpftrace::BPFTraceMap::iterator it,
                                                    uint64_t search_key);
};

}  // namespace stirling
}  // namespace pl

#endif
