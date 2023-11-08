/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/test_utils.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {

using ::testing::ElementsAre;
using ::testing::Pair;

//-----------------------------------------------------------------------------
// A test protocol and parser.
//-----------------------------------------------------------------------------

// This test parser is a simple comma-separated value splitter.

using stream_id_t = uint16_t;
struct TestFrame : public FrameBase {
  std::string msg;

  size_t ByteSize() const override { return sizeof(TestFrame) + msg.size(); }
};

template <>
ParseState ParseFrame(message_type_t /* type */, std::string_view* buf, TestFrame* frame,
                      NoState* /*state*/) {
  size_t pos = buf->find(",");
  if (pos == buf->npos) {
    return ParseState::kNeedsMoreData;
  }

  frame->msg = std::string(buf->substr(0, pos));
  buf->remove_prefix(pos + 1);
  return ParseState::kSuccess;
}

template <>
size_t FindFrameBoundary<TestFrame>(message_type_t /* type */, std::string_view buf,
                                    size_t start_pos, NoState* /*state*/) {
  size_t pos = buf.substr(start_pos).find(",");
  return (pos == buf.npos) ? start_pos : pos;
}

class EventParserTest : public DataStreamBufferTestWrapper, public ::testing::Test {};

// Use test protocol to test basics of EventParser.
TEST_F(EventParserTest, BasicProtocolParsing) {
  absl::flat_hash_map<stream_id_t, std::deque<TestFrame>> word_frames;

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

  AddEvents(events);
  ParseResult<stream_id_t> res = ParseFrames(message_type_t::kRequest, &data_buffer_, &word_frames);

  EXPECT_EQ(ParseState::kSuccess, res.state);
  stream_id_t stream_id = 0;
  EXPECT_THAT(res.frame_positions[stream_id],
              ElementsAre(StartEndPos{0, 7}, StartEndPos{8, 14}, StartEndPos{15, 22},
                          StartEndPos{23, 29}, StartEndPos{30, 35}, StartEndPos{36, 43}));
  EXPECT_EQ(res.end_position, 44);

  std::vector<uint64_t> timestamps;
  for (const auto& stream : word_frames) {
    for (const auto& frame : stream.second) {
      timestamps.push_back(frame.timestamp_ns);
    }
  }
  EXPECT_THAT(timestamps, ElementsAre(0, 1, 1, 2, 3, 4));
}

// TODO(oazizi): Move any protocol specific tests that check for general EventParser behavior here.
// Should help reduce duplication of tests.

}  // namespace protocols
}  // namespace stirling
}  // namespace px
