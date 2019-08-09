#include "src/stirling/http2.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
#include <nghttp2/nghttp2_frame.h>
#include <nghttp2/nghttp2_helper.h>
}

#include "src/common/base/error.h"
#include "src/common/base/status.h"
#include "src/common/testing/testing.h"
#include "src/stirling/testing/utils.h"

namespace pl {
namespace stirling {
namespace http2 {

using ::pl::grpc::MethodInputOutput;
using ::pl::grpc::ServiceDescriptorDatabase;
using ::pl::stirling::testing::GreetServiceFDSet;
using ::pl::testing::proto::EqualsProto;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(UnpackFrameTest, TestVariousCases) {
  struct TestCase {
    std::string input;
    std::string expected_result_buf;
    ParseState expected_parse_state;
  };

  const std::vector<TestCase> test_cases = {
      {std::string(8, ' '), std::string(8, ' '), ParseState::kNeedsMoreData},
      // Non-compressed headers.
      {std::string{"\x0\x0\x3\x1\x4\x0\x0\x0\x1"
                   "a:b",
                   12},
       "", ParseState::kSuccess},
      {std::string{"\x0\x0\x2\x1\x1\x0\x0\x0\x1", 9},
       {"\x0\x0\x2\x1\x1\x0\x0\x0\x1", 9},
       ParseState::kNeedsMoreData},
      {std::string{"\x0\x0\x0\x1\x1\x0\x0\x0\x1", 9}, "", ParseState::kSuccess},
      {std::string{"\x0\x0\x0\x4\x1\x0\x0\x0\x1", 9}, "", ParseState::kIgnored},
  };

  for (const TestCase& c : test_cases) {
    std::string_view buf = c.input;
    Frame frame;
    ParseState s = UnpackFrame(&buf, &frame);
    EXPECT_THAT(s, Eq(c.expected_parse_state));
    EXPECT_THAT(std::string(buf), StrEq(c.expected_result_buf));
  }
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

u8string_view ToU8(std::string_view buf) {
  return u8string_view(reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
}

MATCHER_P2(MatchesTypePayload, t, p, "") {
  return arg.frame.hd.type == t && arg.u8payload == ToU8(p);
}

TEST(UnpackFramesTest, ResultsAreAsExpected) {
  std::string input{
      "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
      "\x0\x0\x3\x1\x4\x0\x0\x0\x1"
      "a:b"  // HEADERS
      "\x0\x0\x3\x9\x4\x0\x0\x0\x1"
      "c:d"  // CONTINUATION
      "\x0\x0\x4\x0\x1\x0\x0\x0\x2"
      "abcd"  // DATA
      "\x0\x0\x1\x1\x1\x0\x0\x0\x3",
      24 + 4 * NGHTTP2_FRAME_HDLEN + 3 + 3 + 4};
  std::string_view buf = input;

  std::deque<Frame> frames;
  ParseResult<size_t> res = Parse(MessageType::kUnknown, buf, &frames);
  EXPECT_THAT(res.state, Eq(ParseState::kNeedsMoreData));
  EXPECT_THAT(res.start_positions, ElementsAre(24, 36, 48));
  EXPECT_THAT(res.end_position, Eq(24 + 3 * NGHTTP2_FRAME_HDLEN + 3 + 3 + 4))
      << "End position does not go into the incomplete frame";
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c:d"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abcd")));
}

// Returns a vector of single GRPCMessage constructed from the input.
std::vector<GRPCMessage> GRPCMsgs(MessageType type, std::string_view msg, NVMap headers) {
  GRPCMessage res;
  res.type = type;
  res.message = msg;
  res.headers = std::move(headers);
  return {std::move(res)};
}

MATCHER_P2(HasMsgAndHdrs, msg, hdrs, "") { return arg.message == msg && arg.headers == hdrs; }

TEST(MatchGRPCReqRespTest, InputsAreMoved) {
  std::map<uint32_t, std::vector<GRPCMessage>> reqs{
      {1u, GRPCMsgs(MessageType::kRequest, "a", {{"h1", "v1"}})},
      {2u, GRPCMsgs(MessageType::kRequest, "b", {{"h2", "v2"}})}};
  std::map<uint32_t, std::vector<GRPCMessage>> resps{
      {0u, GRPCMsgs(MessageType::kResponse, "c", {{"h3", "v3"}})},
      {1u, GRPCMsgs(MessageType::kResponse, "d", {{"h4", "v4"}})}};

  std::vector<GRPCReqResp> matched_msgs = MatchGRPCReqResp(std::move(reqs), std::move(resps));
  ASSERT_THAT(matched_msgs, SizeIs(1));
  const GRPCMessage& req = matched_msgs.begin()->req;
  const GRPCMessage& resp = matched_msgs.begin()->resp;
  EXPECT_EQ("a", req.message);
  EXPECT_EQ("d", resp.message);
}

std::string PackEmptyHeadersFrame(uint8_t flags, uint32_t stream_id) {
  std::string res(NGHTTP2_FRAME_HDLEN, '\0');
  uint8_t* buf = reinterpret_cast<uint8_t*>(res.data());
  nghttp2_frame_hd hd = {};
  hd.length = 0;
  hd.type = NGHTTP2_HEADERS;
  hd.flags = flags;
  hd.stream_id = stream_id;
  nghttp2_frame_pack_frame_hd(buf, &hd);
  return res;
}

std::string PackDataFrame(std::string_view msg, uint8_t flags, uint32_t stream_id) {
  std::string res(NGHTTP2_FRAME_HDLEN + msg.size(), '\0');
  uint8_t* buf = reinterpret_cast<uint8_t*>(res.data());
  nghttp2_frame_hd hd = {};
  hd.length = msg.size();
  hd.type = NGHTTP2_DATA;
  hd.flags = flags;
  hd.stream_id = stream_id;
  nghttp2_frame_pack_frame_hd(buf, &hd);
  res.replace(NGHTTP2_FRAME_HDLEN, msg.size(), msg);
  return res;
}

TEST(StitchGRPCStreamFramesTest, StitchReqsRespsOfDifferentStreams) {
  Inflater inflater;
  std::string input =
      absl::StrCat(PackEmptyHeadersFrame(NGHTTP2_FLAG_END_HEADERS, 1),
                   PackEmptyHeadersFrame(NGHTTP2_FLAG_END_HEADERS, 2), PackDataFrame("abcd", 0, 1),
                   PackDataFrame("abcd", NGHTTP2_FLAG_END_STREAM, 2),
                   PackEmptyHeadersFrame(NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, 1));
  std::map<uint32_t, std::vector<GRPCMessage>> stream_msgs;
  std::deque<Frame> frames;
  ParseResult<size_t> res = Parse(MessageType::kUnknown, input, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);
  ASSERT_THAT(frames, SizeIs(5));

  EXPECT_OK(StitchGRPCStreamFrames(frames, &inflater, &stream_msgs));
  // There should be one gRPC request and response.
  ASSERT_THAT(stream_msgs, ElementsAre(Pair(1, SizeIs(1)), Pair(2, SizeIs(1))));

  const GRPCMessage& req_msg = stream_msgs[2][0];
  EXPECT_EQ(MessageType::kRequest, req_msg.type);
  EXPECT_EQ("abcd", req_msg.message);
  EXPECT_THAT(req_msg.frames, ElementsAre(&frames[1], &frames[3]));

  const GRPCMessage& resp_msg = stream_msgs[1][0];
  EXPECT_EQ(MessageType::kResponse, resp_msg.type);
  EXPECT_EQ("abcd", resp_msg.message);
  // Note we put the HEADERS frames first, and then DATA frames.
  EXPECT_THAT(resp_msg.frames, ElementsAre(&frames[0], &frames[4], &frames[2]));
}

TEST(StitchGRPCStreamFramesTest, InCompleteMessage) {
  Inflater inflater;
  std::string input =
      absl::StrCat(PackEmptyHeadersFrame(NGHTTP2_FLAG_END_HEADERS, 1), PackDataFrame("abcd", 0, 2));
  std::deque<Frame> frames;
  ParseResult<size_t> res = Parse(MessageType::kUnknown, input, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);
  std::map<uint32_t, std::vector<GRPCMessage>> stream_msgs;
  EXPECT_OK(StitchGRPCStreamFrames(frames, &inflater, &stream_msgs));
  EXPECT_THAT(stream_msgs, IsEmpty()) << "There is no END_STREAM in frames, so there is no data";
}

TEST(ParseProtobufsTest, GreeterServiceReqResp) {
  GRPCMessage req;
  req.headers.emplace(":path", "/pl.stirling.testing.Greeter/SayHello");

  ServiceDescriptorDatabase db(GreetServiceFDSet());
  MethodInputOutput in_out = GetProtobufMessages(req, &db);
  ASSERT_NE(nullptr, in_out.input);
  ASSERT_NE(nullptr, in_out.output);
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
