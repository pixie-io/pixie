#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/stirling/bcc_connector.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/source_registry.h"

namespace pl {
namespace stirling {

using pl::types::DataType;
using stirlingpb::Element_State;

void RegisterSources(SourceRegistry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<BCCCPUMetricsConnector>("ebpf_cpu_source");
  registry->RegisterOrDie<ProcStatConnector>("proc_stat_source");
}

// Fake registry for testing in data collector wrapper.
void RegisterFakeSources(SourceRegistry* registry) {
  registry->RegisterOrDie<FakeProcStatConnector>("fake_proc_stat_source");
}

}  // namespace stirling
}  // namespace pl
