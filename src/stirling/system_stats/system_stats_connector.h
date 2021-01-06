#pragma once

#include "src/stirling/core/source_connector.h"
#include "src/stirling/system_stats/system_stats_table.h"

#ifndef __linux__

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(SystemStatsConnector);

}  // namespace stirling
}  // namespace pl
#else

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/core/canonical_types.h"

namespace pl {
namespace stirling {

class SystemStatsConnector : public SourceConnector {
 public:
  static constexpr auto kTables = MakeArray(kProcessStatsTable, kNetworkStatsTable);

  SystemStatsConnector() = delete;
  ~SystemStatsConnector() override = default;

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SystemStatsConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 protected:
  explicit SystemStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables) {
    proc_parser_ = std::make_unique<system::ProcParser>(sysconfig_);
  }

 private:
  void TransferProcessStatsTable(ConnectorContext* ctx, DataTable* data_table);
  void TransferNetworkStatsTable(ConnectorContext* ctx, DataTable* data_table);

  static Status GetNetworkStatsForPod(const system::ProcParser& proc_parser,
                                      const md::PodInfo& pod_info,
                                      const md::K8sMetadataState& k8s_metadata_state,
                                      system::ProcParser::NetworkStats* stats);

  std::unique_ptr<system::ProcParser> proc_parser_;
};

}  // namespace stirling
}  // namespace pl

#endif
