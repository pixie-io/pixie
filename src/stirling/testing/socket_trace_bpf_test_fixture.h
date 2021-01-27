#pragma once

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {
namespace testing {

template <bool TEnableClientSideTracing = false>
class SocketTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_stirling_disable_self_tracing = false;

    source_ = SocketTraceConnector::Create("socket_trace_connector");
    ASSERT_OK(source_->Init());

    RefreshContext();

    source_->InitContext(ctx_.get());

    // InitContext will cause Uprobes to deploy.
    // It won't return until the first set of uprobes has successfully deployed.
    // Sleep an additional second just to be safe.
    sleep(1);
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  void ConfigureBPFCapture(TrafficProtocol protocol, uint64_t role) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->UpdateBPFProtocolTraceRole(protocol, role));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID(pid));
  }

  void RefreshContext() {
    ctx_ = std::make_unique<StandaloneContext>();

    // Normally, Stirling will be setup to think that all traffic is within the cluster,
    // which means only server-side tracing will kick in.
    if (TEnableClientSideTracing) {
      // This makes the Stirling interpret all traffic as leaving the cluster,
      // which means client-side tracing will also apply.
      PL_CHECK_OK(ctx_->SetClusterCIDR("1.2.3.4/32"));
    }
  }

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<StandaloneContext> ctx_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
