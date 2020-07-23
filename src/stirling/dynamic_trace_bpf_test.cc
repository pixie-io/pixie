#include <absl/strings/substitute.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/dynamic_trace_connector.h"
#include "src/stirling/stirling.h"
#include "src/stirling/testing/testing.h"
#include "src/stirling/types.h"

#include "src/stirling/proto/stirling.pb.h"

constexpr std::string_view kClientPath =
    "src/stirling/http2/testing/go_grpc_client/go_grpc_client_/go_grpc_client";
constexpr std::string_view kServerPath =
    "src/stirling/http2/testing/go_grpc_server/go_grpc_server_/go_grpc_server";

namespace pl {
namespace stirling {

using ::google::protobuf::TextFormat;
using ::pl::stirling::testing::ColWrapperSizeIs;
using ::pl::stirling::testing::FindRecordsMatchingPID;
using ::testing::Each;
using ::testing::Ge;
using ::testing::SizeIs;

using LogicalProgram = ::pl::stirling::dynamic_tracing::ir::logical::Program;

enum class TargetKind {
  kBinaryPath,
  kPID,
};

// TODO(yzhao): Create test fixture that wraps the test binaries.
class GoHTTPDynamicTraceTest : public ::testing::Test,
                               public ::testing::WithParamInterface<TargetKind> {
 protected:
  void SetUp() override {
    client_path_ = pl::testing::BazelBinTestFilePath(kClientPath).string();
    server_path_ = pl::testing::BazelBinTestFilePath(kServerPath).string();

    ASSERT_OK(fs::Exists(server_path_));
    ASSERT_OK(fs::Exists(client_path_));

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

  void InitTestFixturesAndRunTestProgram(TargetKind target_kind, const std::string& text_pb) {
    CHECK(TextFormat::ParseFromString(text_pb, &logical_program_));

    logical_program_.mutable_binary_spec()->set_language(
        dynamic_tracing::ir::shared::BinarySpec::GOLANG);

    switch (target_kind) {
      case TargetKind::kBinaryPath:
        logical_program_.mutable_binary_spec()->set_path(server_path_);
        break;
      case TargetKind::kPID:
        logical_program_.mutable_binary_spec()->mutable_upid()->set_pid(s_.child_pid());
        break;
    }

    ASSERT_OK_AND_ASSIGN(bcc_program_, dynamic_tracing::CompileProgram(logical_program_));

    ASSERT_OK_AND_ASSIGN(table_schema_,
                         DynamicDataTableSchema::Create(bcc_program_.perf_buffer_specs.front()));

    data_table_ = std::make_unique<DataTable>(table_schema_->Get());

    ASSERT_OK_AND_ASSIGN(connector_, DynamicTraceConnector::Create(logical_program_));

    ASSERT_OK(connector_->Init());

    ctx_ = std::make_unique<StandaloneContext>();

    ASSERT_OK(c_.Start({client_path_, "-name=PixieLabs", "-count=200",
                        absl::StrCat("-address=localhost:", s_port_)}));
    EXPECT_EQ(0, c_.Wait()) << "Client should be killed";

    connector_->TransferData(ctx_.get(), /*table_num*/ 0, data_table_.get());

    tablets_ = data_table_->ConsumeRecords();
  }

  std::string server_path_;
  std::string client_path_;

  SubProcess c_;
  SubProcess s_;
  int s_port_ = 0;

  LogicalProgram logical_program_;
  dynamic_tracing::BCCProgram bcc_program_;
  std::unique_ptr<DynamicDataTableSchema> table_schema_;
  std::unique_ptr<DataTable> data_table_;
  std::unique_ptr<SourceConnector> connector_;
  std::unique_ptr<StandaloneContext> ctx_;
  std::vector<TaggedRecordBatch> tablets_;
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

constexpr char kReturnValueTraceProgram[] = R"(
outputs {
  name: "probe_readFrameHeader"
  fields: "frame_header_valid"
}
probes: {
  name: "probe_StreamEnded"
  trace_point: {
    symbol: "golang.org/x/net/http2.readFrameHeader"
    type: LOGICAL
  }
  ret_vals {
    id: "frame_header_valid"
    expr: "$2.valid"
  }
  output_actions {
    output_name: "probe_readFrameHeader"
    variable_name: "frame_header_valid"
  }
}
)";

TEST_P(GoHTTPDynamicTraceTest, TraceGolangHTTPClientAndServer) {
  InitTestFixturesAndRunTestProgram(GetParam(), kGRPCTraceProgram);

  ASSERT_THAT(bcc_program_.uprobe_specs, SizeIs(6));
  ASSERT_FALSE(tablets_.empty());

  {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets_[0].records, /*index*/ 0, s_.child_pid());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(200)));

    constexpr size_t kStreamIDIdx = 3;
    constexpr size_t kEndStreamIdx = 4;

    EXPECT_EQ(records[kStreamIDIdx]->Get<types::Int64Value>(0).val, 1);
    EXPECT_EQ(records[kEndStreamIdx]->Get<types::BoolValue>(0).val, false);
  }
}

TEST_P(GoHTTPDynamicTraceTest, TraceReturnValue) {
  InitTestFixturesAndRunTestProgram(GetParam(), kReturnValueTraceProgram);

  ASSERT_THAT(bcc_program_.uprobe_specs, SizeIs(4));
  ASSERT_FALSE(tablets_.empty());

  {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets_[0].records, /*index*/ 0, s_.child_pid());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(1600)));

    constexpr size_t kFrameHeaderValidIdx = 3;

    EXPECT_EQ(records[kFrameHeaderValidIdx]->Get<types::BoolValue>(0).val, true);
  }
}

INSTANTIATE_TEST_SUITE_P(VaryingTracePrograms, GoHTTPDynamicTraceTest,
                         ::testing::Values(TargetKind::kBinaryPath, TargetKind::kPID));

}  // namespace stirling
}  // namespace pl
