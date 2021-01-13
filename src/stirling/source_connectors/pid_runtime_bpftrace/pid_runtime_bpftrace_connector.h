#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bpftrace_wrapper.h"
#include "src/stirling/core/source_connector.h"

#ifndef __linux__
namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(PIDCPUUseBPFTraceConnector);

}  // namespace stirling
}  // namespace pl
#else

namespace pl {
namespace stirling {

class PIDCPUUseBPFTraceConnector : public SourceConnector, public bpf_tools::BPFTraceWrapper {
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

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 private:
  explicit PIDCPUUseBPFTraceConnector(std::string_view name) : SourceConnector(name, kTables) {}

  bpftrace::BPFTraceMap last_result_times_;

  bpftrace::BPFTraceMap::iterator BPFTraceMapSearch(const bpftrace::BPFTraceMap& vector,
                                                    bpftrace::BPFTraceMap::iterator it,
                                                    uint64_t search_key);
};

}  // namespace stirling
}  // namespace pl

#endif
