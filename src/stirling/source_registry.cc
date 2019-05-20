#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bcc_connector.h"
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/cgroup_stats_connector.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/seq_gen_connector.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

void RegisterAllSources(SourceRegistry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<SeqGenConnector>(SeqGenConnector::kName);
  registry->RegisterOrDie<FakeProcStatConnector>(FakeProcStatConnector::kName);
  registry->RegisterOrDie<ProcStatConnector>(ProcStatConnector::kName);
  registry->RegisterOrDie<PIDCPUUseBCCConnector>(PIDCPUUseBCCConnector::kName);
  registry->RegisterOrDie<CPUStatBPFTraceConnector>(CPUStatBPFTraceConnector::kName);
  registry->RegisterOrDie<PIDCPUUseBPFTraceConnector>(PIDCPUUseBPFTraceConnector::kName);
  registry->RegisterOrDie<SocketTraceConnector>(SocketTraceConnector::kName);
  registry->RegisterOrDie<CGroupStatsConnector>(CGroupStatsConnector::kName);
}

}  // namespace stirling
}  // namespace pl
