#include <functional>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

class StirlingBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    registry->RegisterOrDie<SocketTraceConnector>("socket_trace_connector");

    // Make Stirling.
    stirling_ = Stirling::Create(std::move(registry));

    // Set dummy callbacks for agent function.
    stirling_->RegisterDataPushCallback(std::bind(&StirlingBPFTest::AppendData, this,
                                                  std::placeholders::_1, std::placeholders::_2,
                                                  std::placeholders::_3));
  }

  void AppendData(uint64_t table_id, types::TabletID tablet_id,
                  std::unique_ptr<types::ColumnWrapperRecordBatch> record_batch) {
    PL_UNUSED(table_id);
    PL_UNUSED(tablet_id);
    PL_UNUSED(record_batch);
    // A black hole.
  }

  std::unique_ptr<Stirling> stirling_;
};

// Stop Stirling. Meant to be called asynchronously, via a thread.
void AsyncKill(Stirling* stirling) {
  if (stirling != nullptr) {
    stirling->Stop();
  }
}

TEST_F(StirlingBPFTest, CleanupTest) {
  ASSERT_OK(stirling_->RunAsThread());

  // Wait for thread to initialize.
  // TODO(oazizi): This is not good. How do we know how much time is enough?
  std::this_thread::sleep_for(std::chrono::seconds(1));

  EXPECT_GT(SocketTraceConnector::num_attached_probes(), 0);
  EXPECT_GT(SocketTraceConnector::num_open_perf_buffers(), 0);

  std::thread killer_thread = std::thread(&AsyncKill, stirling_.get());

  ASSERT_TRUE(killer_thread.joinable());
  killer_thread.join();

  EXPECT_EQ(SocketTraceConnector::num_attached_probes(), 0);
  EXPECT_EQ(SocketTraceConnector::num_open_perf_buffers(), 0);
}

// TODO(oazizi): If we had a dynamic source that didn't use BPF,
//               this test could be moved to stirling_test.
TEST_F(StirlingBPFTest, DynamicTraceAPI) {
  Status s;

  // Checking status of non-existent trace should return NOT_FOUND.
  s = stirling_->CheckDynamicTraceStatus(/* trace_id */ 1);
  EXPECT_EQ(s.code(), pl::statuspb::Code::NOT_FOUND);

  // Checking status of existent trace should return OK.
  std::string path =
      pl::testing::TestFilePath("src/stirling/obj_tools/testdata/prebuilt_dummy_exe");
  constexpr std::string_view kProgram = R"(
binary_spec {
  path: "$0"
  language: CPP
}
outputs {
  name: "output_table"
  fields: "a"
  fields: "b"
}
probes {
  name: "probe0"
  trace_point {
    symbol: "CanYouFindThis"
    type: LOGICAL
  }
  args {
    id: "a"
    expr: "a"
  }
  args {
    id: "b"
    expr: "b"
  }
  output_actions {
    output_name: "output_table"
    variable_name: "a"
    variable_name: "b"
  }
}
)";

  std::string input_program_str = absl::Substitute(kProgram, path);
  auto trace_program = std::make_unique<dynamic_tracing::ir::logical::Program>();
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(input_program_str, trace_program.get()));

  uint64_t trace_id = stirling_->RegisterDynamicTrace(std::move(trace_program));

  // Immediately after registering, state should be pending.
  // TODO(oazizi): How can we make sure this is not flaky?
  s = stirling_->CheckDynamicTraceStatus(trace_id);
  EXPECT_EQ(s.code(), pl::statuspb::Code::RESOURCE_UNAVAILABLE) << s.ToString();

  do {
    s = stirling_->CheckDynamicTraceStatus(trace_id);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  } while (!s.ok() && s.code() == pl::statuspb::Code::RESOURCE_UNAVAILABLE);

  // Should have finally deployed.
  EXPECT_OK(s);

  // TODO(oazizi): Expand test when RegisterDynamicTrace produces other states.
}

}  // namespace stirling
}  // namespace pl
