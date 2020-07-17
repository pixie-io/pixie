#include <absl/strings/substitute.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_trace_connector.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/output.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/testing/testing.h"
#include "src/stirling/types.h"
#include "src/stirling/utils/linux_headers.h"

#include "src/stirling/proto/stirling.pb.h"

// The binary location cannot be hard-coded because its location depends on -c opt/dbg/fastbuild.
DEFINE_string(go_grpc_client_path, "", "The path to the go greeter client executable.");
DEFINE_string(go_grpc_server_path, "", "The path to the go greeter server executable.");

namespace pl {
namespace stirling {

using ::google::protobuf::TextFormat;
using ::pl::stirling::testing::ColWrapperSizeIs;
using ::pl::stirling::testing::FindRecordsMatchingPID;
using ::pl::stirling::utils::FindOrInstallLinuxHeaders;
using ::pl::stirling::utils::kDefaultHeaderSearchOrder;
using ::testing::Each;
using ::testing::SizeIs;

dynamic_tracing::ir::logical::Program ParseTextProgram(const std::string& text,
                                                       const std::filesystem::path& binary_path) {
  dynamic_tracing::ir::logical::Program logical_program;
  CHECK(TextFormat::ParseFromString(text, &logical_program));
  logical_program.mutable_binary_spec()->set_path(binary_path);
  logical_program.mutable_binary_spec()->set_language(
      dynamic_tracing::ir::shared::BinarySpec::GOLANG);
  return logical_program;
}

// TODO(yzhao): Create test fixture that wraps the test binaries.
class GoHTTPDynamicTraceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    CHECK(!FLAGS_go_grpc_client_path.empty())
        << "--go_grpc_client_path cannot be empty. You should run this test with bazel.";
    CHECK(std::filesystem::exists(std::filesystem::path(FLAGS_go_grpc_client_path)))
        << FLAGS_go_grpc_client_path;

    CHECK(!FLAGS_go_grpc_server_path.empty())
        << "--go_grpc_server_path cannot be empty. You should run this test with bazel.";
    CHECK(std::filesystem::exists(std::filesystem::path(FLAGS_go_grpc_server_path)))
        << FLAGS_go_grpc_server_path;

    server_path_ = FLAGS_go_grpc_server_path;
    client_path_ = FLAGS_go_grpc_client_path;

    ASSERT_OK(FindOrInstallLinuxHeaders({kDefaultHeaderSearchOrder}));

    ASSERT_OK(s_.Start({server_path_}));

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    ASSERT_OK(s_.Stdout(&port_str));
    ASSERT_TRUE(absl::SimpleAtoi(port_str, &s_port_));
    ASSERT_NE(0, s_port_);
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
  }

  std::string server_path_;
  std::string client_path_;

  SubProcess c_;
  SubProcess s_;
  int s_port_ = 0;
};

constexpr char kGRPCTraceProgram[] = R"(
outputs {
  name: "probe_WriteDataPadded_table"
  fields: "stream_id"
  fields: "end_stream"
}
probes: {
  name: "probe_WriteDataPadded"
  trace_point: {
    symbol: "golang.org/x/net/http2.(*Framer).WriteDataPadded"
    type: LOGICAL
  }
  args {
    id: "stream_id"
    expr: "streamID"
  }
  args {
    id: "end_stream"
    expr: "endStream"
  }
  output_actions {
    output_name: "probe_WriteDataPadded_table"
    variable_name: "stream_id"
    variable_name: "end_stream"
  }
}
)";

TEST_F(GoHTTPDynamicTraceTest, TraceGolangHTTPClientAndServer) {
  dynamic_tracing::ir::logical::Program logical_program =
      ParseTextProgram(kGRPCTraceProgram, server_path_);

  ASSERT_OK_AND_ASSIGN(dynamic_tracing::BCCProgram bcc_program,
                       dynamic_tracing::CompileProgram(logical_program));

  ASSERT_THAT(bcc_program.uprobes, SizeIs(3));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DynamicDataTableSchema> table_schema,
                       DynamicDataTableSchema::Create(bcc_program.perf_buffer_specs.front()));

  DataTable data_table(table_schema->Get());

  ASSERT_OK_AND_ASSIGN(auto connector, DynamicTraceConnector::Create(logical_program));
  ASSERT_OK(connector->Init());
  ASSERT_OK(c_.Start(
      {client_path_, "-name=PixieLabs", "-once", absl::StrCat("-address=localhost:", s_port_)}));

  // It seems uprobe is somewhat slower in pushing data into the perf buffer; and this delay ensures
  // that perf buffer gets the data.
  std::this_thread::sleep_for(std::chrono::seconds(1));

  auto ctx = std::make_unique<StandaloneContext>();

  connector->TransferData(ctx.get(), /*table_num*/ 0, &data_table);

  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

  ASSERT_FALSE(tablets.empty());

  {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets[0].records, /*index*/ 0, s_.child_pid());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(1)));

    constexpr size_t kStreamIDIdx = 3;
    constexpr size_t kEndStreamIdx = 4;

    EXPECT_EQ(records[kStreamIDIdx]->Get<types::Int64Value>(0).val, 1);
    EXPECT_EQ(records[kEndStreamIdx]->Get<types::BoolValue>(0).val, false);
  }

  // TODO(yzhao): This fails because kReturnInsts were not supported by BCCWrapper yet.
  // kReturn probe on golang program will crash the server and causes client to fail due to timeout.
  // EXPECT_EQ(0, c_.Wait()) << "Client should exit normally.";
}

}  // namespace stirling
}  // namespace pl
