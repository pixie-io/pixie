#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>

#include "src/stirling/common/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/test_utils.h"

namespace pl {
namespace stirling {
namespace protocols {

using ::testing::ElementsAre;
using ::testing::Pair;

//-----------------------------------------------------------------------------
// A dummy protocol and parser.
//-----------------------------------------------------------------------------

// This dummy parser is a simple comma-separated value splitter.

struct TestFrame : public FrameBase {
  std::string msg;

  size_t ByteSize() const override { return sizeof(TestFrame) + msg.size(); }
};

template <>
ParseState ParseFrame(MessageType /* type */, std::string_view* buf, TestFrame* frame) {
  size_t pos = buf->find(",");
  if (pos == buf->npos) {
    return ParseState::kNeedsMoreData;
  }

  frame->msg = std::string(buf->substr(0, pos));
  buf->remove_prefix(pos + 1);
  return ParseState::kSuccess;
}

template <>
size_t FindFrameBoundary<TestFrame>(MessageType /* type */, std::string_view buf,
                                    size_t start_pos) {
  size_t pos = buf.substr(start_pos).find(",");
  return (pos == buf.npos) ? start_pos : pos;
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

  // Cannot go backwards.
  EXPECT_DEBUG_DEATH(converter.Convert(msgs, 7), "");
}

class EventParserTest : public EventParserTestWrapper, public ::testing::Test {};

// Use dummy protocol to test basics of EventParser.
TEST_F(EventParserTest, BasicProtocolParsing) {
  std::deque<TestFrame> word_frames;

  // clang-format off
  std::vector<std::string> event_messages = {
          "jupiter,satu",
          "rn,neptune,uranus",
          ",",
          "pluto,",
          "mercury,"
  };
  // clang-format on

  std::vector<SocketDataEvent> events = CreateEvents(event_messages);

  AppendEvents(events);
  ParseResult<BufferPosition> res = parser_.ParseFrames(MessageType::kRequest, &word_frames);

  EXPECT_EQ(ParseState::kSuccess, res.state);
  EXPECT_THAT(res.frame_positions, ElementsAre(StartEndPos<BufferPosition>{{0, 0}, {0, 7}},
                                               StartEndPos<BufferPosition>{{0, 8}, {1, 2}},
                                               StartEndPos<BufferPosition>{{1, 3}, {1, 10}},
                                               StartEndPos<BufferPosition>{{1, 11}, {2, 0}},
                                               StartEndPos<BufferPosition>{{3, 0}, {3, 5}},
                                               StartEndPos<BufferPosition>{{4, 0}, {4, 7}}));
  EXPECT_EQ((BufferPosition{5, 0}), res.end_position);

  std::vector<uint64_t> timestamps;
  for (const auto& frame : word_frames) {
    timestamps.push_back(frame.timestamp_ns);
  }
  EXPECT_THAT(timestamps, ElementsAre(0, 1, 1, 2, 3, 4));
}

// TODO(oazizi): Move any protocol specific tests that check for general EventParser behavior here.
// Should help reduce duplication of tests.

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
