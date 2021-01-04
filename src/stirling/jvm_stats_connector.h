#pragma once

#ifndef __linux__

#include "src/stirling/core/source_connector.h"

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
#include "src/stirling/core/source_connector.h"
#include "src/stirling/jvm_stats_table.h"
#include "src/stirling/utils/java.h"
#include "src/stirling/utils/proc_tracker.h"

namespace pl {
namespace stirling {

// Reads and parses the hsperfdata file created by JVM, and exports them into a data table.
//
// Hsperfdata is a JVM feature that exports JVM performance stats into a memory-mapped file under
// /tmp directory. It's supported by almost all JVM from major vendors.
class JVMStatsConnector : public SourceConnector {
 public:
  static constexpr auto kTables = MakeArray(kJVMStatsTable);
  static constexpr int kTableNum = SourceConnector::TableNum(kTables, kJVMStatsTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new JVMStatsConnector(name));
  }

  Status InitImpl() override { return Status::OK(); }
  Status StopImpl() override { return Status::OK(); }

  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;

 private:
  explicit JVMStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables) {}

  // Finds the UPIDs of newly-created processes as monitoring targets.
  void FindJavaUPIDs(const ConnectorContext& ctx);

  // Exports JVM performance metrics to data table.
  Status ExportStats(const md::UPID& upid, const std::filesystem::path& hsperf_data_path,
                     DataTable* data_table) const;

  // Keeps track of the currently-running processes. Used to find the newly-created processes.
  ProcTracker proc_tracker_;

  // Records the PIDs of previously scanned Java processes, and their hsperfdata file path.
  struct JavaProcInfo {
    // How many times we have failed to export stats for this process. Once this reaches a limit,
    // the process will no longer be monitored.
    int export_failure_count = 0;
    std::filesystem::path hsperf_data_path;
  };
  absl::flat_hash_map<md::UPID, JavaProcInfo> java_procs_;
};

}  // namespace stirling
}  // namespace pl

#endif
