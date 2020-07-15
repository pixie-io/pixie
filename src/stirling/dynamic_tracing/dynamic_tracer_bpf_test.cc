#include <absl/strings/substitute.h>
#include <google/protobuf/text_format.h>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/dynamic_tracer.h"
#include "src/stirling/utils/linux_headers.h"

DEFINE_string(go_grpc_client_path, "", "The path to the go greeter client executable.");
DEFINE_string(go_grpc_server_path, "", "The path to the go greeter server executable.");

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::google::protobuf::TextFormat;
using ::pl::stirling::bpf_tools::BCCWrapper;
using ::pl::stirling::utils::FindOrInstallLinuxHeaders;
using ::pl::stirling::utils::kDefaultHeaderSearchOrder;
using ::testing::SizeIs;

ir::logical::Program ParseTextProgram(const std::string& text,
                                      const std::filesystem::path& binary_path) {
  ir::logical::Program logical_program;
  CHECK(TextFormat::ParseFromString(text, &logical_program));
  logical_program.set_binary_path(binary_path);
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
  name: "WriteDataPaddedTrace"
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
    output_name: "WriteDataPaddedTrace"
    variable_name: "stream_id"
    variable_name: "end_stream"
  }
  printks { scalar: "stream_id" }
  printks { scalar: "end_stream" }
}
)";

TEST_F(GoHTTPDynamicTraceTest, TraceGolangHTTPClientAndServer) {
  ir::logical::Program logical_program = ParseTextProgram(kGRPCTraceProgram, server_path_);

  ASSERT_OK_AND_ASSIGN(BCCProgram bcc_program, CompileProgram(logical_program));

  PL_LOG_VAR(bcc_program.code);

  ASSERT_THAT(bcc_program.uprobes, SizeIs(3));

  BCCWrapper bcc_wrapper;

  ASSERT_OK(DeployBCCProgram(bcc_program, &bcc_wrapper));

  ASSERT_OK(c_.Start(
      {client_path_, "-name=PixieLabs", "-once", absl::StrCat("-address=localhost:", s_port_)}));

  // TODO(yzhao): This fails because kReturnInsts were not supported by BCCWrapper yet.
  // kReturn probe on golang program will crash the server and causes client to fail due to timeout.
  // EXPECT_EQ(0, c_.Wait()) << "Client should exit normally.";
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
