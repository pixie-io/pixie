#include "src/stirling/event_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nghttp2/nghttp2.h>
extern "C" {
#include <nghttp2/nghttp2_frame.h>
}

#include <deque>

#include "src/stirling/http2.h"

namespace pl {
namespace stirling {

using ::pl::stirling::http2::Frame;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

bool operator==(const BufferPosition& lhs, const BufferPosition& rhs) {
  return lhs.seq_num == rhs.seq_num && lhs.offset == rhs.offset;
}

TEST(EventParserTest, ParsesMessagesConsumeNoDataFromBuf) {
  EventParser<Frame> parser;
  std::deque<Frame> frames;
  parser.Append("PRI * HTTP/2.0", 0u);
  ParseResult<BufferPosition> res = parser.ParseMessages(MessageType::kRequest, &frames);

  EXPECT_EQ(ParseState::kNeedsMoreData, res.state);
  EXPECT_THAT(res.start_positions, IsEmpty());
  EXPECT_EQ((BufferPosition{0, 0}), res.end_position);
}

}  // namespace stirling
}  // namespace pl
