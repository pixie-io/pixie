#pragma once

#include "src/stirling/core/source_connector.h"

#ifndef __linux__

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(DynamicBPFTraceConnector);

}  // namespace stirling
}  // namespace pl

#else

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"
#include "src/stirling/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace pl {
namespace stirling {

class DynamicBPFTraceConnector : public SourceConnector, public bpf_tools::BPFTraceWrapper {
 public:
  static StatusOr<std::unique_ptr<SourceConnector> > Create(
      std::string_view source_name,
      const dynamic_tracing::ir::logical::TracepointDeployment::Tracepoint& tracepoint);

  DynamicBPFTraceConnector() = delete;
  ~DynamicBPFTraceConnector() override = default;

 protected:
  explicit DynamicBPFTraceConnector(std::string_view source_name,
                                    std::unique_ptr<DynamicDataTableSchema> table_schema,
                                    std::string_view script);
  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 private:
  void HandleEvent(uint8_t* data);

  std::string name_;
  std::unique_ptr<DynamicDataTableSchema> table_schema_;
  std::string script_;

  // The types according to the BPFTrace printf format.
  std::vector<bpftrace::Field> output_fields_;

  // Used by HandleEvent so that when a callback is triggered, HandleEvent knows the context.
  DataTable* data_table_ = nullptr;
};

}  // namespace stirling
}  // namespace pl

#endif
