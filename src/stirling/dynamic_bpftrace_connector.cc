#ifdef __linux__

#include "src/stirling/dynamic_bpftrace_connector.h"

#include <vector>

#include <absl/memory/memory.h>

namespace pl {
namespace stirling {

std::unique_ptr<SourceConnector> DynamicBPFTraceConnector::Create(
    std::string_view source_name, const dynamic_tracing::ir::logical::BPFTrace& bpftrace) {
  std::vector<DataElement> dummy_elements;
  const DataTableSchema dummy_table_schema_array[] = {{source_name, dummy_elements}};
  const ArrayView<DataTableSchema> dummy_table_schemas(dummy_table_schema_array);
  return absl::WrapUnique<DynamicBPFTraceConnector>(
      new DynamicBPFTraceConnector(source_name, dummy_table_schemas, bpftrace.program()));
}

DynamicBPFTraceConnector::DynamicBPFTraceConnector(std::string_view source_name,
                                                   const ArrayView<DataTableSchema>& table_schemas,
                                                   std::string_view script)
    : SourceConnector(source_name, table_schemas), script_(script) {}

Status DynamicBPFTraceConnector::InitImpl() {
  return error::Unimplemented("DynamicBPFTraceConnector::InitImpl()");
}

Status DynamicBPFTraceConnector::StopImpl() {
  return error::Unimplemented("DynamicBPFTraceConnector::StopImpl()");
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
