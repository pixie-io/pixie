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

using ::google::protobuf::Message;
using ::google::protobuf::util::MessageDifferencer;
using ::pl::stirling::http2::Frame;
using ::pl::stirling::http2::GRPCMessage;
using ::pl::stirling::http2::UnpackFrames;
using ::pl::stirling::http2::UnpackGRPCMessage;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;

std::string JoinStream(const std::map<uint64_t, socket_data_event_t>& stream) {
  std::string result;
  for (const auto& [seq_num, event] : stream) {
    PL_UNUSED(seq_num);
    result.append(event.msg, event.attr.msg_size);
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

  SubProcess c({c_path, "-name=PixieLabs", "-once"});
  EXPECT_OK(c.Start());
  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";
  s.Kill();
  EXPECT_EQ(9, s.Wait()) << "Server should have been killed.";

  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(connector.get());
  ASSERT_NE(nullptr, socket_trace_connector);

  const int kTableNum = 2;
  socket_trace_connector->ReadPerfBuffer(kTableNum);
  ASSERT_GE(socket_trace_connector->TestOnlyHTTP2Streams().size(), 1);

  const HTTP2Stream h2_stream = socket_trace_connector->TestOnlyHTTP2Streams().begin()->second;
  {
    std::string send_string = JoinStream(h2_stream.send_events);
    std::string_view send_buf = send_string;
    std::vector<std::unique_ptr<Frame>> frames;
    EXPECT_OK(UnpackFrames(&send_buf, &frames));
    ASSERT_THAT(frames, SizeIs(3));

    EXPECT_THAT(*frames[0], IsFrameType(NGHTTP2_HEADERS));
    EXPECT_THAT(*frames[1], IsFrameType(NGHTTP2_DATA));
    EXPECT_THAT(*frames[2], IsFrameType(NGHTTP2_HEADERS));
    EXPECT_EQ(1, frames[0]->frame.hd.stream_id);
    EXPECT_EQ(1, frames[1]->frame.hd.stream_id);
    EXPECT_EQ(1, frames[2]->frame.hd.stream_id);

    EXPECT_THAT(Headers(*frames[0]),
                ElementsAre(Pair(":status", "200"), Pair("content-type", "application/grpc")));
    EXPECT_THAT(Headers(*frames[2]),
                ElementsAre(Pair("grpc-message", ""), Pair("grpc-status", "0")));

    std::string_view buf = frames[1]->payload;
    GRPCMessage message = {};
    EXPECT_OK(UnpackGRPCMessage(&buf, &message));

    testing::HelloReply received_reply;
    EXPECT_TRUE(received_reply.ParseFromString(message.message));

    testing::HelloReply expected_reply;
    expected_reply.set_message("Hello PixieLabs");
    EXPECT_TRUE(MessageDifferencer::Equals(received_reply, expected_reply));
  }
  {
    std::string recv_string = JoinStream(h2_stream.recv_events);
    std::string_view recv_buf = recv_string;
    std::vector<std::unique_ptr<Frame>> frames;
    constexpr size_t kHTTP2ClientConnectPrefaceSizeInBytes = 24;
    recv_buf.remove_prefix(kHTTP2ClientConnectPrefaceSizeInBytes);
    EXPECT_OK(UnpackFrames(&recv_buf, &frames));
    ASSERT_THAT(frames, SizeIs(2));

    EXPECT_THAT(*frames[0], IsFrameType(NGHTTP2_HEADERS));
    EXPECT_THAT(*frames[1], IsFrameType(NGHTTP2_DATA));
    EXPECT_EQ(1, frames[0]->frame.hd.stream_id);
    EXPECT_EQ(1, frames[1]->frame.hd.stream_id);

    EXPECT_THAT(Headers(*frames[0]),
                ElementsAre(Pair(":authority", "localhost:50051"), Pair(":method", "POST"),
                            Pair(":path", "/pl.stirling.testing.Greeter/SayHello"),
                            Pair(":scheme", "http"), Pair("content-type", "application/grpc"),
                            Pair("grpc-timeout", _),  // The value keeps changing, just ignore it.
                            Pair("te", "trailers"), Pair("user-agent", "grpc-go/1.19.0")));

    std::string_view buf = frames[1]->payload;
    GRPCMessage message = {};
    EXPECT_OK(UnpackGRPCMessage(&buf, &message));

    testing::HelloRequest received_req;
    EXPECT_TRUE(received_req.ParseFromString(message.message));

    testing::HelloRequest expected_req;
    expected_req.set_name("PixieLabs");
    EXPECT_TRUE(MessageDifferencer::Equals(received_req, expected_req));
  }
}

}  // namespace stirling
}  // namespace pl
