#include "src/stirling/common/event_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>

namespace pl {
namespace stirling {

using ::testing::ElementsAre;

//-----------------------------------------------------------------------------
// A dummy protocol and parser.
//-----------------------------------------------------------------------------

// This dummy parser is a simple comma-separated value splitter.

struct TestFrame {
  std::string msg;
  uint64_t timestamp_ns;
};

template <>
ParseResult<size_t> Parse(MessageType /* type */, std::string_view buf,
                          std::deque<TestFrame>* messages) {
  ParseResult<size_t> result;

  size_t position = 0;
  result.state = ParseState::kNeedsMoreData;

  for (size_t i = 0; i < buf.size(); ++i) {
    if (buf[i] == ',' || buf[i] == ';') {
      TestFrame frame;
      frame.msg = std::string(buf.substr(position, i - position));
      result.start_positions.push_back(position);
      messages->push_back(frame);
      position = i + 1;

      if (buf[i] == ';') {
        result.state = ParseState::kSuccess;
        break;
      }
    }
  }

  result.end_position = position;
  return result;
}

template <>
size_t FindMessageBoundary<TestFrame>(MessageType /* type */, std::string_view buf,
                                      size_t start_pos) {
  size_t pos = buf.substr(start_pos).find(",");
  return (pos == buf.npos) ? start_pos : pos;
}

bool operator==(const BufferPosition& lhs, const BufferPosition& rhs) {
  return lhs.seq_num == rhs.seq_num && lhs.offset == rhs.offset;
}

// Use dummy protocol to test basics of EventParser and its position conversions.
TEST(EventParserTest, BasicPositionConversions) {
  EventParser<TestFrame> parser;
  std::deque<TestFrame> word_frames;
  parser.Append("jupiter,satu", 0u);
  parser.Append("rn,neptune,plu", 0u);
  ParseResult<BufferPosition> res = parser.ParseMessages(MessageType::kRequest, &word_frames);

  EXPECT_EQ(ParseState::kNeedsMoreData, res.state);
  EXPECT_THAT(res.start_positions,
              ElementsAre(BufferPosition{0, 0}, BufferPosition{0, 8}, BufferPosition{1, 3}));
  EXPECT_EQ((BufferPosition{1, 11}), res.end_position);
}

}  // namespace stirling
}  // namespace pl
