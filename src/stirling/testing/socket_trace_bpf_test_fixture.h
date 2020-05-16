#pragma once

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/client_server_system.h"

namespace pl {
namespace stirling {
namespace testing {

template <bool TEnableClientSideTracing = false>
class SocketTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_stirling_disable_self_tracing = false;

    // Normally, Stirling will be setup to think that all traffic is within the cluster,
    // which means only server-side tracing will kick in.
    if (TEnableClientSideTracing) {
      // This makes the Stirling interpret all traffic as leaving the cluster,
      // which means client-side tracing will also apply.
      FLAGS_stirling_cluster_cidr = "1.2.3.4/32";
    }

    source_ = SocketTraceConnector::Create("socket_trace_connector");
    ASSERT_OK(source_->Init());

    // Create a context to pass into each TransferData() in the test, using a dummy ASID.
    static constexpr uint32_t kASID = 1;
    auto agent_metadata_state = std::make_shared<md::AgentMetadataState>(kASID);
    ctx_ = std::make_unique<ConnectorContext>(std::move(agent_metadata_state));
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  void ConfigureCapture(TrafficProtocol protocol, EndpointRole role) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->UpdateProtocolTraceRole(protocol, role));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID(pid));
  }

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<ConnectorContext> ctx_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
