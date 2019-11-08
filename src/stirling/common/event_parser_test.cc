#include "src/stirling/common/event_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>

#include "src/stirling/common/utils.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::DataEventWithTimeSpan;
using ::testing::ElementsAre;
using ::testing::Pair;

//-----------------------------------------------------------------------------
// A dummy protocol and parser.
//-----------------------------------------------------------------------------

// This dummy parser is a simple comma-separated value splitter.

struct TestFrame {
  std::string msg;
  TimeSpan time_span;
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

  SocketDataEvent event0 = DataEventWithTimeSpan("jupiter,satu", {0, 1});
  SocketDataEvent event1 = DataEventWithTimeSpan("rn,neptune,plu", {2, 3});
  SocketDataEvent event2 = DataEventWithTimeSpan(",", {3, 4});
  SocketDataEvent event3 = DataEventWithTimeSpan("aaa,", {4, 5});
  SocketDataEvent event4 = DataEventWithTimeSpan("bbb,", {6, 7});

  parser.Append(event0);
  parser.Append(event1);
  parser.Append(event2);
  parser.Append(event3);
  parser.Append(event4);
  ParseResult<BufferPosition> res = parser.ParseMessages(MessageType::kRequest, &word_frames);

  EXPECT_EQ(ParseState::kNeedsMoreData, res.state);
  EXPECT_THAT(res.start_positions,
              ElementsAre(BufferPosition{0, 0}, BufferPosition{0, 8}, BufferPosition{1, 3},
                          BufferPosition{1, 11}, BufferPosition{3, 0}, BufferPosition{4, 0}));
  EXPECT_EQ((BufferPosition{5, 0}), res.end_position);

  std::vector<std::pair<uint64_t, uint64_t>> time_spans;
  for (const auto& frame : word_frames) {
    time_spans.push_back({frame.time_span.begin_ns, frame.time_span.end_ns});
  }
  EXPECT_THAT(time_spans,
              ElementsAre(Pair(0, 1), Pair(0, 3), Pair(2, 3), Pair(2, 4), Pair(4, 5), Pair(6, 7)));
}

}  // namespace stirling
}  // namespace pl
