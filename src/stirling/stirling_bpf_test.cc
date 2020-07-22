#include <functional>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(dummy_go_binary, "", "The path to dummy_go_binary.");

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
    record_batches_.push_back(std::move(record_batch));
  }

  Status WaitForStatus(uint64_t trace_id) {
    Status s;
    do {
      s = stirling_->CheckDynamicTraceStatus(trace_id);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (!s.ok() && s.code() == pl::statuspb::Code::RESOURCE_UNAVAILABLE);

    return s;
  }

  std::unique_ptr<dynamic_tracing::ir::logical::Program> Prepare(std::string_view program,
                                                                 std::string_view path) {
    std::string input_program_str = absl::Substitute(program, path);
    auto trace_program = std::make_unique<dynamic_tracing::ir::logical::Program>();
    CHECK(google::protobuf::TextFormat::ParseFromString(input_program_str, trace_program.get()));
    return trace_program;
  }

  std::optional<int> FindFieldIndex(const stirlingpb::TableSchema& schema,
                                    std::string_view field_name) {
    int idx = 0;
    for (const auto& e : schema.elements()) {
      if (e.name() == field_name) {
        return idx;
      }
      ++idx;
    }
    return {};
  }

  std::unique_ptr<Stirling> stirling_;
  std::vector<std::unique_ptr<types::ColumnWrapperRecordBatch>> record_batches_;
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

  auto trace_program = Prepare(kProgram, path);

  uint64_t trace_id = stirling_->RegisterDynamicTrace(std::move(trace_program));

  // Immediately after registering, state should be pending.
  // TODO(oazizi): How can we make sure this is not flaky?
  s = stirling_->CheckDynamicTraceStatus(trace_id);
  EXPECT_EQ(s.code(), pl::statuspb::Code::RESOURCE_UNAVAILABLE) << s.ToString();

  // Should deploy.
  ASSERT_OK(WaitForStatus(trace_id));

  // TODO(oazizi): Expand test when RegisterDynamicTrace produces other states.
}

TEST_F(StirlingBPFTest, string_test) {
  // Run tracing target.
  SubProcess process;
  CHECK(!FLAGS_dummy_go_binary.empty())
      << "--dummy_go_binary cannot be empty. You should run this test with bazel.";
  CHECK(std::filesystem::exists(std::filesystem::path(FLAGS_dummy_go_binary)))
      << FLAGS_dummy_go_binary;
  ASSERT_OK(process.Start({FLAGS_dummy_go_binary}));

  std::string path = pl::testing::TestFilePath(FLAGS_dummy_go_binary);
  constexpr std::string_view kProgram = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
outputs {
  name: "output_table"
  fields: "something"
  fields: "name"
}
probes {
  name: "probe0"
  trace_point {
    symbol: "main.SaySomethingTo"
    type: LOGICAL
  }
  args {
    id: "name"
    expr: "name"
  }
  args {
    id: "something"
    expr: "something"
  }
  output_actions {
    output_name: "output_table"
    variable_name: "something"
    variable_name: "name"
  }
}

)";

  auto trace_program = Prepare(kProgram, path);
  uint64_t trace_id = stirling_->RegisterDynamicTrace(std::move(trace_program));

  // Should deploy.
  ASSERT_OK(WaitForStatus(trace_id));

  stirlingpb::Publish publication;
  stirling_->GetPublishProto(&publication);

  const stirlingpb::TableSchema* schema;
  stirlingpb::Subscribe subscription;

  for (const auto& info_class : publication.published_info_classes()) {
    auto sub_info_class = subscription.add_subscribed_info_classes();
    sub_info_class->MergeFrom(info_class);
    sub_info_class->set_subscribed(false);
    if (info_class.name() == "output_table") {
      sub_info_class->set_subscribed(true);
      schema = &sub_info_class->schema();
    }
  }
  ASSERT_OK(stirling_->SetSubscription(subscription));

  // Get field indexes for the two columns we want.
  ASSERT_NE(schema, nullptr);
  int name_field_idx = FindFieldIndex(*schema, "name").value();
  int something_field_idx = FindFieldIndex(*schema, "something").value();

  // Run Stirling data collector.
  ASSERT_OK(stirling_->RunAsThread());

  while (record_batches_.empty()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  stirling_->Stop();

  types::ColumnWrapperRecordBatch& rb = *record_batches_[0];
  EXPECT_EQ(rb[something_field_idx]->Get<types::StringValue>(0), "Hello");
  EXPECT_EQ(rb[name_field_idx]->Get<types::StringValue>(0), "pixienaut");
}

}  // namespace stirling
}  // namespace pl
