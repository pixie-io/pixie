#include "src/stirling/dynamic_bpftrace_connector.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::pl::stirling::dynamic_tracing::ir::logical::BPFTrace;

TEST(DynamicBPFTraceConnectorTest, Create) {
  BPFTrace bpftrace;
  DynamicBPFTraceConnector::Create("test", bpftrace);
}

}  // namespace stirling
}  // namespace pl
