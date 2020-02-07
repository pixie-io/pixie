#ifdef __linux__

#include "src/stirling/jvm_stats_connector.h"
#include "src/stirling/jvm_stats_table.h"

namespace pl {
namespace stirling {

void JVMStatsConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                         DataTable* data_table) {
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  PL_UNUSED(ctx);

  RecordBuilder<&kJVMStatsTable> r(data_table);

  r.Append<kTimeIdx>({});
  r.Append<kUPIDIdx>({});
  r.Append<kYoungGCTimeIdx>({});
  r.Append<kFullGCTimeIdx>({});
  r.Append<kUsedHeapSizeIdx>({});
  r.Append<kTotalHeapSizeIdx>({});
  r.Append<kMaxHeapSizeIdx>({});
}

}  // namespace stirling
}  // namespace pl

#endif
