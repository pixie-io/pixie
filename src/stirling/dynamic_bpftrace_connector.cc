#ifdef __linux__

#include "src/stirling/dynamic_bpftrace_connector.h"

#include <utility>
#include <vector>

#include <absl/memory/memory.h>

namespace pl {
namespace stirling {

std::unique_ptr<SourceConnector> DynamicBPFTraceConnector::Create(
    std::string_view source_name, const dynamic_tracing::ir::logical::BPFTrace& bpftrace) {
  std::unique_ptr<DynamicDataTableSchema> table_schema =
      DynamicDataTableSchema::Create(absl::StrCat(source_name, "_output"), bpftrace);
  return std::unique_ptr<SourceConnector>(
      new DynamicBPFTraceConnector(source_name, std::move(table_schema), bpftrace.program()));
}

DynamicBPFTraceConnector::DynamicBPFTraceConnector(
    std::string_view source_name, std::unique_ptr<DynamicDataTableSchema> table_schema,
    std::string_view script)
    : SourceConnector(source_name, ArrayView<DataTableSchema>(&table_schema->Get(), 1)),
      table_schema_(std::move(table_schema)),
      script_(script) {}

Status DynamicBPFTraceConnector::InitImpl() { return Deploy(script_, /*params*/ {}); }

Status DynamicBPFTraceConnector::StopImpl() {
  BPFTraceWrapper::Stop();
  return Status::OK();
}

void DynamicBPFTraceConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                                DataTable* data_table) {
  PL_UNUSED(ctx);
  PL_UNUSED(table_num);
  PL_UNUSED(data_table);
}

}  // namespace stirling
}  // namespace pl

#endif
