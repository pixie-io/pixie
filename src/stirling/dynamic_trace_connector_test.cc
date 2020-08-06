#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/output.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

#include "src/stirling/proto/stirling.pb.h"

constexpr std::string_view kBinaryPath =
    "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary";

namespace pl {
namespace stirling {

using pl::types::ColumnWrapperRecordBatch;
using pl::types::TabletID;

const stirlingpb::TableSchema* g_table_schema;

void StirlingCallback(uint64_t table_id, TabletID tablet_id,
                      std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
  PrintRecordBatch("DynamicTraceSource", *g_table_schema, *record_batch);
  PL_UNUSED(table_id);
  PL_UNUSED(tablet_id);
}

constexpr std::string_view kProgramSpec = R"(
binary_spec {
  path: "$0"
  language: GOLANG
}
outputs {
  name: "probe0_table"
  fields: "arg0"
  fields: "arg1"
  fields: "arg2"
  fields: "arg3"
  fields: "arg4"
  fields: "arg5"
  fields: "retval0"
  fields: "retval1"
}
probes: {
  name: "probe0"
  trace_point: {
    symbol: "main.MixedArgTypes"
    type: LOGICAL
  }
  args {
    id: "arg0"
    expr: "i1"
  }
  args {
    id: "arg1"
    expr: "i2"
  }
  args {
    id: "arg2"
    expr: "i3"
  }
  args {
    id: "arg3"
    expr: "b1"
  }
  args {
    id: "arg4"
    expr: "b2.B0"
  }
  args {
    id: "arg5"
    expr: "b2.B3"
  }
  ret_vals {
    id: "retval0"
    expr: "$$6"
  }
  ret_vals {
    id: "retval1"
    expr: "$$7.B1"
  }
  output_actions {
    output_name: "probe0_table"
    variable_name: "arg0"
    variable_name: "arg1"
    variable_name: "arg2"
    variable_name: "arg3"
    variable_name: "arg4"
    variable_name: "arg5"
    variable_name: "retval0"
    variable_name: "retval1"
  }
}
)";

TEST(DynamicTraceSource, dynamic_trace_source) {
  std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();

  // Make Stirling.
  std::unique_ptr<Stirling> stirling = Stirling::Create(std::move(registry));

  // Set a dummy callback function (normally this would be in the agent).
  stirling->RegisterDataPushCallback(StirlingCallback);

  std::string program_str =
      absl::Substitute(kProgramSpec, pl::testing::BazelBinTestFilePath(kBinaryPath).string());
  auto trace_program = std::make_unique<dynamic_tracing::ir::logical::TracepointDeployment>();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(program_str, trace_program.get()));
  sole::uuid trace_id = sole::uuid4();
  stirling->RegisterTracepoint(trace_id, std::move(trace_program));

  // Wait for the probe to deploy.
  StatusOr<stirlingpb::Publish> s;
  do {
    s = stirling->GetTracepointInfo(trace_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  } while (!s.ok() && s.code() == pl::statuspb::Code::RESOURCE_UNAVAILABLE);

  // OK state should persist.
  ASSERT_OK_AND_ASSIGN(stirlingpb::Publish trace_pub, stirling->GetTracepointInfo(trace_id));

  ASSERT_OK(stirling->SetSubscription(pl::stirling::SubscribeToAllInfoClasses(trace_pub)));

  // Run Stirling data collector.
  ASSERT_OK(stirling->RunAsThread());

  std::this_thread::sleep_for(std::chrono::seconds(3));

  stirling->Stop();
  stirling->WaitForThreadJoin();
}

}  // namespace stirling
}  // namespace pl
