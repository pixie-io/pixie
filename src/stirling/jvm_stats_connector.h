#pragma once

#ifndef __linux__

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(JVMStatsConnector);

}  // namespace stirling
}  // namespace pl

#else

#include <absl/container/flat_hash_set.h>
#include <map>
#include <memory>
#include <string_view>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/base_types.h"
#include "src/stirling/jvm_stats_table.h"
#include "src/stirling/source_connector.h"
#include "src/stirling/utils/java.h"

namespace pl {
namespace stirling {

class JVMStatsConnector : public SourceConnector {
 public:
  static constexpr auto kTables = MakeArray(kJVMStatsTable);
  static constexpr int kTableNum = SourceConnector::TableNum(kTables, kJVMStatsTable);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{5000};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{5000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new JVMStatsConnector(name));
  }
  Status InitImpl() override { return Status::OK(); }
  Status StopImpl() override { return Status::OK(); }

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 private:
  explicit JVMStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod) {
    proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  }

  // Returns UPIDs that has not been scanned in the previous iteration of transferring data.
  // TODO(yzhao): Merge with SocketTraceConnector::FindNewUPIDs().
  absl::flat_hash_set<md::UPID> FindNewUPIDs(const ConnectorContext& ctx);
  Status ExportStats(const md::UPID& upid, DataTable* data_table) const;

  std::unique_ptr<system::ProcParser> proc_parser_;

  // Records the UPIDs that have been scanned in the previous iteration of transferring data.
  absl::flat_hash_set<md::UPID> prev_scanned_upids_;
  absl::flat_hash_set<md::UPID> prev_scanned_java_upids_;
};

}  // namespace stirling
}  // namespace pl

#endif
