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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <absl/container/flat_hash_map.h>
#include "src/stirling/source_connectors/socket_tracer/protocols/common/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/parse.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

// Captured request packet after issuing the following in cqlsh:
//   SELECT * FROM system.schema_keyspaces ;
constexpr uint8_t kQueryFrame[] = {
    0x04, 0x00, 0x00, 0x06, 0x07, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x27, 0x53,
    0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x2a, 0x20, 0x46, 0x52, 0x4f, 0x4d, 0x20, 0x73,
    0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x5f, 0x6b,
    0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73, 0x20, 0x3b, 0x00, 0x01, 0x34, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x08, 0x00, 0x05, 0x9d, 0xaf, 0x91, 0xd4, 0xc0, 0x5c};

// For reference, a few more captured frames, in case they become useful.
// constexpr uint8_t kOptionsFrame[] = {
//        0x04, 0x00, 0x00, 0x61, 0x05, 0x00, 0x00, 0x00,
//        0x00
//};
//
// constexpr uint8_t kSupportedFrame[] = {
//        0x84, 0x00, 0x00, 0x6b, 0x06, 0x00, 0x00, 0x00,
//        0x60, 0x00, 0x03, 0x00, 0x11, 0x50, 0x52, 0x4f,
//        0x54, 0x4f, 0x43, 0x4f, 0x4c, 0x5f, 0x56, 0x45,
//        0x52, 0x53, 0x49, 0x4f, 0x4e, 0x53, 0x00, 0x03,
//        0x00, 0x04, 0x33, 0x2f, 0x76, 0x33, 0x00, 0x04,
//        0x34, 0x2f, 0x76, 0x34, 0x00, 0x09, 0x35, 0x2f,
//        0x76, 0x35, 0x2d, 0x62, 0x65, 0x74, 0x61, 0x00,
//        0x0b, 0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53,
//        0x53, 0x49, 0x4f, 0x4e, 0x00, 0x02, 0x00, 0x06,
//        0x73, 0x6e, 0x61, 0x70, 0x70, 0x79, 0x00, 0x03,
//        0x6c, 0x7a, 0x34, 0x00, 0x0b, 0x43, 0x51, 0x4c,
//        0x5f, 0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e,
//        0x00, 0x01, 0x00, 0x05, 0x33, 0x2e, 0x34, 0x2e,
//        0x34
//};

class CQLParserTest : public ::testing::Test {};

TEST_F(CQLParserTest, Basic) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kQueryFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);
  ASSERT_EQ(TotalDequeSize(frames), 1);
  std::deque<Frame> expected_stream = frames[6];
  EXPECT_EQ(expected_stream[0].hdr.version & 0x80, 0);
  EXPECT_EQ(expected_stream[0].hdr.version & 0x7f, 4);
  EXPECT_EQ(expected_stream[0].hdr.flags, 0);
  EXPECT_EQ(expected_stream[0].hdr.stream, 6);
  EXPECT_EQ(expected_stream[0].hdr.opcode, Opcode::kQuery);
  EXPECT_EQ(expected_stream[0].hdr.length, 60);
  EXPECT_THAT(expected_stream[0].msg,
              testing::HasSubstr("SELECT * FROM system.schema_keyspaces ;"));
}

TEST_F(CQLParserTest, NeedsMoreData) {
  std::string_view frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kQueryFrame));
  frame_view.remove_suffix(10);

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kNeedsMoreData);
  ASSERT_EQ(TotalDequeSize(frames), 0);
}

TEST_F(CQLParserTest, BadOpcode) {
  constexpr uint8_t kBadOpcodeFrame[] = {0x04, 0x00, 0x00, 0x06, 0xff, 0x00,
                                         0x00, 0x00, 0x02, 0x00, 0x00};

  std::string_view frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kBadOpcodeFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  ASSERT_EQ(TotalDequeSize(frames), 0);
}

TEST_F(CQLParserTest, LengthTooLarge) {
  // Length is 0x020000000
  constexpr uint8_t kBadLengthFrame[] = {0x04, 0x00, 0x00, 0x06, 0x07, 0x20,
                                         0x00, 0x00, 0x00, 0x00, 0x00};

  std::string_view frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kBadLengthFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  ASSERT_EQ(TotalDequeSize(frames), 0);
}

TEST_F(CQLParserTest, LengthNegative) {
  // Length is 0xf0000000
  constexpr uint8_t kBadLengthFrame[] = {0x04, 0x00, 0x00, 0x06, 0x07, 0xf0,
                                         0x00, 0x00, 0x00, 0x00, 0x00};

  std::string_view frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kBadLengthFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  ASSERT_EQ(TotalDequeSize(frames), 0);
}

TEST_F(CQLParserTest, VersionTooOld) {
  // Version is set to 2.
  constexpr uint8_t kBadLengthFrame[] = {0x02, 0x00, 0x00, 0x06, 0x07, 0x00,
                                         0x00, 0x00, 0x02, 0x00, 0x00};

  std::string_view frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kBadLengthFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  ASSERT_EQ(TotalDequeSize(frames), 0);
}

TEST_F(CQLParserTest, VersionTooNew) {
  // Version is set to 5.
  constexpr uint8_t kBadLengthFrame[] = {0x05, 0x00, 0x00, 0x06, 0x07, 0x00,
                                         0x00, 0x00, 0x02, 0x00, 0x00};

  std::string_view frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kBadLengthFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  ASSERT_EQ(TotalDequeSize(frames), 0);
}

}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
