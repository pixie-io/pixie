#include "src/stirling/http2.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
#include <nghttp2/nghttp2_frame.h>
}

#include "src/common/base/error.h"

namespace pl {
namespace stirling {
namespace http2 {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
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
  return arg->frame.hd.type == t && arg->u8payload == ToU8(p);
}

TEST(UnpackFramesTest, BrokenAndIgnoredFramesAreSkipped) {
  std::string input{
      "\x0\x0\x3\x1\x4\x0\x0\x0\x1"
      "a:b"  // HEADERS
      "\x0\x0\x3\x9\x4\x0\x0\x0\x1"
      "c:d"  // CONTINUATION
      "\x0\x0\x4\x0\x1\x0\x0\x0\x2"
      "abcd"  // DATA
      "\x0\x0\x1\x1\x1\x0\x0\x0\x3",
      4 * NGHTTP2_FRAME_HDLEN + 3 + 3 + 4};
  std::string_view buf = input;

  std::deque<std::unique_ptr<Frame>> frames;
  ParseResult<size_t> res = Parse(MessageType::kUnknown, buf, &frames);
  EXPECT_THAT(res.state, Eq(ParseState::kNeedsMoreData));
  EXPECT_THAT(res.end_position, Eq(3 * NGHTTP2_FRAME_HDLEN + 3 + 3 + 4));
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c:d"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abcd")));
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
