#include <gmock/gmock.h>
#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include <nghttp2/nghttp2_frame.h>
}

#include <thread>

#include "src/common/subprocess/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/data_table.h"
#include "src/stirling/grpc.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/greeter_client.h"
#include "src/stirling/testing/greeter_server.h"
#include "src/stirling/testing/proto/greet.grpc.pb.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::pl::stirling::testing::GreeterClient;
using ::pl::stirling::testing::GreeterService;
using ::pl::stirling::testing::HelloReply;
using ::pl::stirling::testing::HelloRequest;
using ::pl::stirling::testing::ServiceRunner;
using ::pl::testing::proto::EqualsProto;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::HasSubstr;
using ::testing::MatchesRegex;
using ::testing::SizeIs;

constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
constexpr DataTableSchema kHTTPTable = SocketTraceConnector::kHTTPTable;
constexpr uint32_t kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
constexpr uint32_t kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
constexpr uint32_t kHTTPReqHeadersIdx = kHTTPTable.ColIndex("http_req_headers");
constexpr uint32_t kHTTPRespHeadersIdx = kHTTPTable.ColIndex("http_resp_headers");
constexpr uint32_t kHTTPPIDIdx = kHTTPTable.ColIndex("pid");
constexpr uint32_t kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
constexpr uint32_t kHTTPRemotePortIdx = kHTTPTable.ColIndex("remote_port");
constexpr uint32_t kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");

std::vector<size_t> FindRecordIdxMatchesPid(const ColumnWrapperRecordBatch& http_record, int pid) {
  std::vector<size_t> res;
  for (size_t i = 0; i < http_record[kHTTPPIDIdx]->Size(); ++i) {
    if (http_record[kHTTPPIDIdx]->Get<types::Int64Value>(i).val == pid) {
      res.push_back(i);
    }
  }
  return res;
}

TEST(GRPCTraceBPFTest, TestGolangGrpcService) {
  // Force disable protobuf parsing to output the binary protobuf in record batch.
  // Also ensure test remain passing when the default changes.
  FLAGS_enable_parsing_protobufs = false;

  constexpr char kBaseDir[] = "src/stirling/testing";
  std::string s_path =
      TestEnvironment::PathToTestDataFile(absl::StrCat(kBaseDir, "/go_greeter_server"));
  std::string c_path =
      TestEnvironment::PathToTestDataFile(absl::StrCat(kBaseDir, "/go_greeter_client"));
  SubProcess s({s_path});
  EXPECT_OK(s.Start());

  // TODO(yzhao): We have to install probes after starting server. Otherwise we will run into
  // failures when detaching them. This might be relevant to probes are inherited by child process
  // when fork() and execvp().
  std::unique_ptr<SourceConnector> connector =
      SocketTraceConnector::Create("socket_trace_connector");
  auto* socket_trace_connector = static_cast<SocketTraceConnector*>(connector.get());
  ASSERT_NE(nullptr, socket_trace_connector);
  ASSERT_OK(connector->Init());

  // TODO(yzhao): Add a --count flag to greeter client so we can test the case of multiple RPC calls
  // (multiple HTTP2 streams).
  SubProcess c({c_path, "-name=PixieLabs", "-once"});
  EXPECT_OK(c.Start());

  EXPECT_OK(socket_trace_connector->TestOnlySetTargetPID(c.child_pid()));

  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";
  s.Kill();
  EXPECT_EQ(9, s.Wait()) << "Server should have been killed.";

  DataTable data_table(SocketTraceConnector::kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  connector->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);
  for (const auto& col : record_batch) {
    // Sometimes connect() returns 0, so we might have data from requester and responder.
    ASSERT_GE(col->Size(), 1);
  }
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPid(record_batch, c.child_pid());
  // We should get exactly one record.
  ASSERT_THAT(target_record_indices, SizeIs(1));
  const size_t target_record_idx = target_record_indices.front();

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      MatchesRegex(":authority: localhost:50051\n"
                   ":method: POST\n"
                   ":path: /pl.stirling.testing.Greeter/SayHello\n"
                   ":scheme: http\n"
                   "content-type: application/grpc\n"
                   "grpc-timeout: [0-9]+u\n"
                   "te: trailers\n"
                   "user-agent: grpc-go/.+"));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      MatchesRegex(":status: 200\n"
                   "content-type: application/grpc\n"
                   "grpc-message: \n"
                   "grpc-status: 0"));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(target_record_idx)),
      HasSubstr("127.0.0.1"));
  EXPECT_EQ(50051, record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(target_record_idx).val);

  HelloReply received_reply;
  std::string msg = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(target_record_idx);
  EXPECT_TRUE(received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes)));
  EXPECT_THAT(received_reply, EqualsProto(R"proto(message: "Hello PixieLabs")proto"));
}

class GRPCTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Force disable protobuf parsing to output the binary protobuf in record batch.
    // Also ensure test remain passing when the default changes.
    FLAGS_enable_parsing_protobufs = false;
    source_ = SocketTraceConnector::Create("bcc_grpc_trace");
    ASSERT_OK(source_->Init());

    auto* socket_trace_connector = static_cast<SocketTraceConnector*>(source_.get());
    ASSERT_NE(nullptr, socket_trace_connector);
    EXPECT_OK(socket_trace_connector->TestOnlySetTargetPID(getpid()));

    server_ = runner_.RunService(&service_);
    auto* server_ptr = server_.get();
    server_thread_ = std::thread([server_ptr]() { server_ptr->Wait(); });
    client_ = std::make_unique<GreeterClient>(absl::StrCat("127.0.0.1:", runner_.ports().back()));
  }

  void TearDown() override {
    ASSERT_OK(source_->Stop());
    server_->Shutdown();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  std::unique_ptr<SourceConnector> source_;
  GreeterService service_;
  ServiceRunner runner_;
  std::unique_ptr<::grpc::Server> server_;
  std::thread server_thread_;
  std::unique_ptr<GreeterClient> client_;
};

TEST_F(GRPCTest, BasicTracingForCPP) {
  HelloRequest req;
  HelloReply resp;

  req.set_name("pixielabs");
  ::grpc::Status st = client_->SayHello(req, &resp);
  EXPECT_OK(st) << st.error_message();

  DataTable data_table(SocketTraceConnector::kHTTPTable);
  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  source_->TransferData(/* ctx */ nullptr, kHTTPTableNum, &data_table);

  const std::vector<size_t> target_record_indices = FindRecordIdxMatchesPid(record_batch, getpid());
  // We should get exactly one record.
  ASSERT_THAT(target_record_indices, SizeIs(1));
  const size_t target_record_idx = target_record_indices.front();

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      MatchesRegex(":authority: 127.0.0.1:[0-9]+\n"
                   ":method: POST\n"
                   ":path: /pl.stirling.testing.Greeter/SayHello\n"
                   ":scheme: http\n"
                   "accept-encoding: identity,gzip\n"
                   "content-type: application/grpc\n"
                   "grpc-accept-encoding: identity,deflate,gzip\n"
                   "te: trailers\n"
                   "user-agent: .*"));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      MatchesRegex(":status: 200\n"
                   "accept-encoding: identity,gzip\n"
                   "content-type: application/grpc\n"
                   "grpc-accept-encoding: identity,deflate,gzip\n"
                   "grpc-status: 0"));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(target_record_idx)),
      HasSubstr("127.0.0.1"));
  EXPECT_EQ(runner_.ports().back(),
            record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(target_record_idx).val);

  HelloReply received_reply;
  std::string msg = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(target_record_idx);
  EXPECT_TRUE(received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes)));
  EXPECT_THAT(received_reply, EqualsProto(R"proto(message: "Hello pixielabs!")proto"));
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
