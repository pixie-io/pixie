#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <thread>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/data_table.h"
#include "src/stirling/protocols/http2/grpc.h"
#include "src/stirling/protocols/http2/testing/greeter_server.h"
#include "src/stirling/protocols/http2/testing/proto/greet.grpc.pb.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/common.h"

constexpr std::string_view kClientPath =
    "src/stirling/protocols/http2/testing/go_grpc_client/go_grpc_client_/go_grpc_client";
constexpr std::string_view kServerPath =
    "src/stirling/protocols/http2/testing/go_grpc_server/go_grpc_server_/go_grpc_server";

namespace pl {
namespace stirling {

using ::grpc::Channel;
using ::pl::stirling::grpc::kGRPCMessageHeaderSizeInBytes;
using ::pl::stirling::protocols::http2::testing::HelloReply;
using ::pl::stirling::protocols::http2::testing::HelloRequest;
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::testing::proto::EqualsProto;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;

HelloReply GetHelloReply(const ColumnWrapperRecordBatch& record_batch, const size_t idx) {
  HelloReply received_reply;
  std::string msg = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
  if (!msg.empty()) {
    received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes));
  }
  return received_reply;
}

HelloRequest GetHelloRequest(const ColumnWrapperRecordBatch& record_batch, const size_t idx) {
  HelloRequest received_reply;
  std::string msg = record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);
  if (!msg.empty()) {
    received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes));
  }
  return received_reply;
}

class GRPCTraceGoTest : public ::testing::Test {
 protected:
  GRPCTraceGoTest() : data_table_(kHTTPTable), ctx_(std::make_unique<StandaloneContext>()) {}

  void LaunchServer(bool use_https) {
    client_path_ = pl::testing::BazelBinTestFilePath(kClientPath).string();
    server_path_ = pl::testing::BazelBinTestFilePath(kServerPath).string();

    ASSERT_OK(fs::Exists(server_path_));
    ASSERT_OK(fs::Exists(client_path_));

    std::string https_flag = use_https ? "--https=true" : "--https=false";
    ASSERT_OK(s_.Start({server_path_, https_flag}));
    LOG(INFO) << "Server PID: " << s_.child_pid();

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    ASSERT_OK(s_.Stdout(&port_str));
    ASSERT_TRUE(absl::SimpleAtoi(port_str, &s_port_));
    ASSERT_NE(0, s_port_);
  }

  void InitSocketTraceConnector() {
    // Force disable protobuf parsing to output the binary protobuf in record batch.
    // Also ensure test remain passing when the default changes.
    FLAGS_stirling_enable_parsing_protobufs = false;

    // TODO(yzhao): We have to install probes after starting server. Otherwise we will run into
    // failures when detaching them. This might be relevant to probes are inherited by child process
    // when fork() and execvp().
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    PL_CHECK_OK(connector_->Init());
    connector_->InitContext(ctx_.get());
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
    EXPECT_OK(connector_->Stop());
  }

  std::string server_path_;
  std::string client_path_;

  // Create a context to pass into each TransferData() in the test, using a dummy ASID.
  static constexpr uint32_t kASID = 1;

  DataTable data_table_;
  SubProcess s_;
  int s_port_ = -1;
  std::unique_ptr<StandaloneContext> ctx_;
  std::unique_ptr<SourceConnector> connector_;
};

class GRPCTraceTest : public GRPCTraceGoTest, public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override { GRPCTraceGoTest::InitSocketTraceConnector(); }
};

TEST_P(GRPCTraceTest, CaptureRPCTraceRecord) {
  // Server is launched after initializing socket tracer, which verifies that uprobes
  // are dynamically attached.
  GRPCTraceGoTest::LaunchServer(GetParam());

  connector_->InitContext(ctx_.get());

  SubProcess c;
  const std::string https_flag = GetParam() ? "--https=true" : "--https=false";
  ASSERT_OK(c.Start({client_path_, https_flag, "-once", "-name=PixieLabs",
                     absl::StrCat("-address=localhost:", s_port_)}));
  LOG(INFO) << "Client PID: " << c.child_pid();
  EXPECT_EQ(0, c.Wait());

  connector_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);

  std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, s_.child_pid());
  ASSERT_GE(target_record_indices.size(), 1);

  // We should get exactly one record.
  const size_t idx = target_record_indices.front();
  const std::string scheme_text = GetParam() ? R"(":scheme":"https")" : R"(":scheme":"http")";

  md::UPID upid(record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).val);
  std::filesystem::path proc_pid_path =
      system::Config::GetInstance().proc_path() / std::to_string(s_.child_pid());
  ASSERT_OK_AND_ASSIGN(int64_t pid_start_time, system::GetPIDStartTimeTicks(proc_pid_path));
  md::UPID expected_upid(/* asid */ 0, s_.child_pid(), pid_start_time);
  EXPECT_EQ(upid, expected_upid);

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(idx)),
      AllOf(HasSubstr(absl::Substitute(R"(":authority":"localhost:$0")", s_port_)),
            HasSubstr(R"(":method":"POST")"), HasSubstr(scheme_text),
            HasSubstr(absl::StrCat(R"(":scheme":)", GetParam() ? R"("https")" : R"("http")")),
            HasSubstr(R"("content-type":"application/grpc")"), HasSubstr(R"("grpc-timeout")"),
            HasSubstr(R"("te":"trailers","user-agent")")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(idx)),
      AllOf(HasSubstr(R"(":status":"200")"), HasSubstr(R"("content-type":"application/grpc")"),
            HasSubstr(R"("grpc-message":"")"), HasSubstr(R"("grpc-status":"0"})")));
  EXPECT_THAT(std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(idx)),
              AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
  EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_EQ(0, record_batch[kHTTPMinorVersionIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_THAT(GetHelloReply(record_batch, idx),
              EqualsProto(R"proto(message: "Hello PixieLabs")proto"));
}

INSTANTIATE_TEST_SUITE_P(SecurityModeTest, GRPCTraceTest, ::testing::Values(true, false));

}  // namespace stirling
}  // namespace pl
