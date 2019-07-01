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
    Status expected_status;
  };

  const std::vector<TestCase> test_cases = {
      {std::string(8, ' '), std::string(8, ' '), error::ResourceUnavailable("got: 8 must be >= 9")},
      // Non-compressed headers.
      {std::string{"\x0\x0\x3\x1\x4\x0\x0\x0\x1"
                   "a:b",
                   12},
       "", Status::OK()},
      {std::string{"\x0\x0\x2\x1\x1\x0\x0\x0\x1", 9},
       {"\x0\x0\x2\x1\x1\x0\x0\x0\x1", 9},
       error::ResourceUnavailable("got: 9 must be >= 11")},
      {std::string{"\x0\x0\x0\x1\x1\x0\x0\x0\x1", 9}, "", Status::OK()},
  };

  for (const TestCase& c : test_cases) {
    std::string_view buf = c.input;
    Frame frame;
    Status s = UnpackFrame(&buf, &frame);
    EXPECT_THAT(s.code(), Eq(c.expected_status.code()));
    EXPECT_THAT(s.msg(), HasSubstr(c.expected_status.msg()));
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

  std::vector<std::unique_ptr<Frame>> frames;
  Status s = UnpackFrames(&buf, &frames);
  EXPECT_TRUE(error::IsResourceUnavailable(s));
  EXPECT_THAT(s.msg(), HasSubstr("got: 9 must be >= 10"));
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c:d"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abcd")));
  EXPECT_EQ(buf, std::string_view("\x0\x0\x1\x1\x1\x0\x0\x0\x3", NGHTTP2_FRAME_HDLEN));
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
