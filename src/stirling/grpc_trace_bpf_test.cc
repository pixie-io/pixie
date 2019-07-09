#include <gmock/gmock.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

extern "C" {
#include <nghttp2/nghttp2_frame.h>
}

#include "src/common/subprocess/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/http2.h"
#include "src/stirling/socket_trace_connector.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/stirling/testing/proto/greet.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace stirling {
namespace http2 {

using ::google::protobuf::Message;
using ::google::protobuf::util::MessageDifferencer;
using ::pl::stirling::testing::HelloReply;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::MatchesRegex;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;

constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
constexpr DataTableSchema kHTTPTable = SocketTraceConnector::kHTTPTable;
constexpr uint32_t kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
constexpr uint32_t kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
constexpr uint32_t kHTTPHeaderIdx = kHTTPTable.ColIndex("http_headers");
constexpr uint32_t kHTTPTGIDIdx = kHTTPTable.ColIndex("tgid");
constexpr uint32_t kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
constexpr uint32_t kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");

std::vector<size_t> FindRecordIdxMatchesPid(const ColumnWrapperRecordBatch& http_record, int pid) {
  std::vector<size_t> res;
  for (size_t i = 0; i < http_record[kHTTPTGIDIdx]->Size(); ++i) {
    if (http_record[kHTTPTGIDIdx]->Get<types::Int64Value>(i).val == pid) {
      res.push_back(i);
    }
  }
  return res;
}

TEST(GRPCTraceBPFTest, TestGolangGrpcService) {
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
  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(connector.get());
  ASSERT_NE(nullptr, socket_trace_connector);
  ASSERT_OK(connector->Init());
  // This resets the probe to server side, so this is not subject to update in
  // SocketTraceConnector::InitImpl().
  EXPECT_OK(socket_trace_connector->Configure(kProtocolHTTP2,
                                              kSocketTraceSendResp | kSocketTraceRecvReq));

  // TODO(yzhao): Add a --count flag to greeter client so we can test the case of multiple RPC calls
  // (multiple HTTP2 streams).
  SubProcess c({c_path, "-name=PixieLabs", "-once"});
  EXPECT_OK(c.Start());
  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";
  s.Kill();
  EXPECT_EQ(9, s.Wait()) << "Server should have been killed.";

  ColumnWrapperRecordBatch record_batch;
  InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 1, &record_batch);

  connector->TransferData(kHTTPTableNum, &record_batch);
  for (const auto& col : record_batch) {
    // Sometimes connect() returns 0, so we might have data from requester and responder.
    ASSERT_GE(col->Size(), 1);
  }
  const std::vector<size_t> server_record_indices =
      FindRecordIdxMatchesPid(record_batch, s.child_pid());
  // We should get exactly one record.
  ASSERT_THAT(server_record_indices, SizeIs(1));
  const size_t server_record_idx = server_record_indices.front();

  EXPECT_EQ(s.child_pid(),
            record_batch[kHTTPTGIDIdx]->Get<types::Int64Value>(server_record_idx).val);
  EXPECT_THAT(std::string(record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(server_record_idx)),
              MatchesRegex(":authority: localhost:50051\n"
                           ":method: POST\n"
                           ":path: /pl.stirling.testing.Greeter/SayHello\n"
                           ":scheme: http\n"
                           "content-type: application/grpc\n"
                           "grpc-timeout: [0-9]+u\n"
                           "te: trailers\n"
                           "user-agent: grpc-go/.+"));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(server_record_idx)),
      HasSubstr("127.0.0.1"));
  EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(server_record_idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(server_record_idx).val);

  HelloReply received_reply, expected_reply;
  expected_reply.set_message("Hello PixieLabs");
  EXPECT_TRUE(ParseProtobuf(
      record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(server_record_idx), &received_reply));
  EXPECT_TRUE(MessageDifferencer::Equals(received_reply, expected_reply));
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
