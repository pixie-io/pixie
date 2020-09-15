#pragma once

#include "src/stirling/source_connector.h"

#ifndef __linux__

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(DynamicBPFTraceConnector);

}  // namespace stirling
}  // namespace pl

#else

#include <memory>
#include <string>

#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"

namespace pl {
namespace stirling {

class DynamicBPFTraceConnector : public SourceConnector {
 public:
  static std::unique_ptr<SourceConnector> Create(
      std::string_view source_name, const dynamic_tracing::ir::logical::BPFTrace& bpftrace);

  DynamicBPFTraceConnector() = delete;
  ~DynamicBPFTraceConnector() override = default;

 protected:
  explicit DynamicBPFTraceConnector(std::string_view source_name,
                                    const ArrayView<DataTableSchema>& table_schemas,
                                    std::string_view script);
  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 private:
  std::string name_;
  std::string script_;
};

}  // namespace stirling
}  // namespace pl

#endif
