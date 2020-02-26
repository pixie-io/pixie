#include "src/stirling/common/event_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>

#include "src/stirling/common/utils.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::DataEventWithTimestamp;
using ::testing::ElementsAre;
using ::testing::Pair;

//-----------------------------------------------------------------------------
// A dummy protocol and parser.
//-----------------------------------------------------------------------------

// This dummy parser is a simple comma-separated value splitter.

struct TestFrame {
  std::string msg;
  uint64_t timestamp_ns;
};

template <>
ParseResult<size_t> ParseFrames(MessageType /* type */, std::string_view buf,
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
size_t FindFrameBoundary<TestFrame>(MessageType /* type */, std::string_view buf,
                                    size_t start_pos) {
  size_t pos = buf.substr(start_pos).find(",");
  return (pos == buf.npos) ? start_pos : pos;
}

bool operator==(const BufferPosition& lhs, const BufferPosition& rhs) {
  return lhs.seq_num == rhs.seq_num && lhs.offset == rhs.offset;
}

TEST(PositionConverterTest, Basic) {
  PositionConverter converter;

  std::vector<std::string_view> msgs = {"0", "12", "345", "6", "789"};

  BufferPosition bp;
  bp = converter.Convert(msgs, 0);
  EXPECT_EQ(bp, (BufferPosition{0, 0}));

  bp = converter.Convert(msgs, 6);
  EXPECT_EQ(bp, (BufferPosition{3, 0}));

  bp = converter.Convert(msgs, 6);
  EXPECT_EQ(bp, (BufferPosition{3, 0}));

  bp = converter.Convert(msgs, 8);
  EXPECT_EQ(bp, (BufferPosition{4, 1}));

  bp = converter.Convert(msgs, 8);
  EXPECT_EQ(bp, (BufferPosition{4, 1}));

#if DCHECK_IS_ON()
  // Cannot go backwards.
  EXPECT_DEATH(converter.Convert(msgs, 7), "");
#endif
}

// Use dummy protocol to test basics of EventParser and its position conversions.
TEST(EventParserTest, BasicPositionConversions) {
  EventParser<TestFrame> parser;
  std::deque<TestFrame> word_frames;

  SocketDataEvent event0 = DataEventWithTimestamp("jupiter,satu", 0);
  SocketDataEvent event1 = DataEventWithTimestamp("rn,neptune,plu", 2);
  SocketDataEvent event2 = DataEventWithTimestamp(",", 3);
  SocketDataEvent event3 = DataEventWithTimestamp("aaa,", 4);
  SocketDataEvent event4 = DataEventWithTimestamp("bbb,", 6);

  parser.Append(event0);
  parser.Append(event1);
  parser.Append(event2);
  parser.Append(event3);
  parser.Append(event4);
  ParseResult<BufferPosition> res = parser.ParseFrames(MessageType::kRequest, &word_frames);

  EXPECT_EQ(ParseState::kNeedsMoreData, res.state);
  EXPECT_THAT(res.start_positions,
              ElementsAre(BufferPosition{0, 0}, BufferPosition{0, 8}, BufferPosition{1, 3},
                          BufferPosition{1, 11}, BufferPosition{3, 0}, BufferPosition{4, 0}));
  EXPECT_EQ((BufferPosition{5, 0}), res.end_position);

  std::vector<uint64_t> timestamps;
  for (const auto& frame : word_frames) {
    timestamps.push_back(frame.timestamp_ns);
  }
  EXPECT_THAT(timestamps, ElementsAre(0, 0, 2, 2, 4, 6));
}

}  // namespace stirling
}  // namespace pl
