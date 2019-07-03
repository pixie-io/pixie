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
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;

std::string JoinStream(const std::map<uint64_t, SocketDataEvent>& stream) {
  std::string result;
  for (const auto& [seq_num, event] : stream) {
    PL_UNUSED(seq_num);
    result.append(event.msg);
  }
  return result;
}

std::map<std::string, std::string> Headers(const Frame& frame) {
  std::map<std::string, std::string> result;
  for (size_t i = 0; i < frame.frame.headers.nvlen; ++i) {
    std::string name(reinterpret_cast<const char*>(frame.frame.headers.nva[i].name),
                     frame.frame.headers.nva[i].namelen);
    std::string value(reinterpret_cast<const char*>(frame.frame.headers.nva[i].value),
                      frame.frame.headers.nva[i].valuelen);
    result[name] = value;
  }
  return result;
}

MATCHER_P(IsFrameType, t, "") { return arg.frame.hd.type == t; }
MATCHER_P(HasStreamID, t, "") { return arg.frame.hd.stream_id == t; }

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
  ASSERT_OK(connector->Init());

  // TODO(yzhao): Add a --count flag to greeter client so we can test the case of multiple RPC calls
  // (multiple HTTP2 streams).
  SubProcess c({c_path, "-name=PixieLabs", "-once"});
  EXPECT_OK(c.Start());
  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";
  s.Kill();
  EXPECT_EQ(9, s.Wait()) << "Server should have been killed.";

  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(connector.get());
  ASSERT_NE(nullptr, socket_trace_connector);

  socket_trace_connector->ReadPerfBuffer(SocketTraceConnector::kHTTPTableNum);
  ASSERT_GE(socket_trace_connector->TestOnlyStreams().size(), 1);

  const ConnectionTracker h2_stream = socket_trace_connector->TestOnlyStreams().begin()->second;
  {
    std::string send_string = JoinStream(h2_stream.send_data().events);
    std::string_view send_buf = send_string;
    std::deque<Frame> frames;

    ParseResult<size_t> s = Parse(MessageType::kMixed, send_buf, &frames);
    EXPECT_EQ(ParseState::kSuccess, s.state);

    EXPECT_THAT(frames, ElementsAre(IsFrameType(NGHTTP2_HEADERS), IsFrameType(NGHTTP2_DATA),
                                    IsFrameType(NGHTTP2_HEADERS)));
    EXPECT_THAT(frames, ElementsAre(HasStreamID(1), HasStreamID(1), HasStreamID(1)));

    std::map<uint32_t, std::vector<GRPCMessage>> stream_msgs;
    EXPECT_OK(StitchGRPCStreamFrames(&frames, &stream_msgs));
    EXPECT_THAT(frames, IsEmpty());
    ASSERT_THAT(stream_msgs, ElementsAre(Pair(1, SizeIs(1))));

    const GRPCMessage& resp = stream_msgs[1].front();
    EXPECT_TRUE(resp.parse_succeeded);
    EXPECT_THAT(resp.type, Eq(MessageType::kResponses));
    EXPECT_THAT(resp.headers,
                ElementsAre(Pair(":status", "200"), Pair("content-type", "application/grpc"),
                            Pair("grpc-message", ""), Pair("grpc-status", "0")));

    testing::HelloReply received_reply;
    EXPECT_TRUE(resp.ParseProtobuf(&received_reply));

    testing::HelloReply expected_reply;
    expected_reply.set_message("Hello PixieLabs");
    EXPECT_TRUE(MessageDifferencer::Equals(received_reply, expected_reply));
  }
  {
    std::string recv_string = JoinStream(h2_stream.recv_data().events);
    std::string_view recv_buf = recv_string;
    std::deque<Frame> frames;
    constexpr size_t kHTTP2ClientConnectPrefaceSizeInBytes = 24;
    recv_buf.remove_prefix(kHTTP2ClientConnectPrefaceSizeInBytes);

    ParseResult<size_t> s = Parse(MessageType::kUnknown, recv_buf, &frames);
    EXPECT_EQ(ParseState::kSuccess, s.state);
    ASSERT_THAT(frames, SizeIs(2));

    EXPECT_THAT(frames, ElementsAre(IsFrameType(NGHTTP2_HEADERS), IsFrameType(NGHTTP2_DATA)));
    EXPECT_THAT(frames, ElementsAre(HasStreamID(1), HasStreamID(1)));

    std::map<uint32_t, std::vector<GRPCMessage>> stream_msgs;
    EXPECT_OK(StitchGRPCStreamFrames(&frames, &stream_msgs));
    EXPECT_THAT(frames, IsEmpty());
    ASSERT_THAT(stream_msgs, ElementsAre(Pair(1, SizeIs(1))));

    const GRPCMessage& req = stream_msgs[1].front();
    EXPECT_TRUE(req.parse_succeeded);
    EXPECT_THAT(req.type, Eq(MessageType::kRequests));
    EXPECT_THAT(req.headers,
                ElementsAre(Pair(":authority", "localhost:50051"), Pair(":method", "POST"),
                            Pair(":path", "/pl.stirling.testing.Greeter/SayHello"),
                            Pair(":scheme", "http"), Pair("content-type", "application/grpc"),
                            Pair("grpc-timeout", _),  // The value keeps changing, just ignore it.
                            Pair("te", "trailers"), Pair("user-agent", "grpc-go/1.19.0")));

    testing::HelloRequest received_req;
    EXPECT_TRUE(req.ParseProtobuf(&received_req));

    testing::HelloRequest expected_req;
    expected_req.set_name("PixieLabs");
    EXPECT_TRUE(MessageDifferencer::Equals(received_req, expected_req));
  }
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
