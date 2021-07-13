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

#include <algorithm>
#include <deque>
#include <random>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/parse.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

bool operator==(const Packet& lhs, const Packet& rhs) {
  if (lhs.msg.compare(rhs.msg) != 0) {
    return false;
  }
  if (lhs.correlation_id != rhs.correlation_id) {
    return false;
  }
  return true;
}

using ::testing::ElementsAre;

constexpr uint8_t kProduceRequest[] = {
    0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x63, 0x6f,
    0x6e, 0x73, 0x6f, 0x6c, 0x65, 0x2d, 0x70, 0x72, 0x6f, 0x64, 0x75, 0x63, 0x65, 0x72, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x05, 0xdc, 0x02, 0x12, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x73, 0x74, 0x61,
    0x72, 0x74, 0x2d, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x02, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e, 0xff, 0xff, 0xff, 0xff, 0x02,
    0xc0, 0xde, 0x91, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8,
    0x2d, 0xaa, 0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8, 0x2d, 0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x38, 0x00, 0x00, 0x00,
    0x01, 0x2c, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6d, 0x79, 0x20, 0x66, 0x69, 0x72,
    0x73, 0x74, 0x20, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x00, 0x00, 0x00, 0x00};

constexpr uint8_t kProduceResponse[] = {
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x12, 0x71, 0x75, 0x69,
    0x63, 0x6b, 0x73, 0x74, 0x61, 0x72, 0x74, 0x2d, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73,
    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr uint8_t kMetaDataRequest[] = {
    0x00, 0x00, 0x00, 0x1c, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0d, 0x61, 0x64,
    0x6d, 0x69, 0x6e, 0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x2d, 0x31, 0x00, 0x01, 0x01, 0x00, 0x00};

constexpr uint8_t kMetaDataResponse[] = {
    0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x0a, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, 0x00, 0x00, 0x23, 0x84,
    0x00, 0x00, 0x17, 0x5a, 0x65, 0x76, 0x76, 0x4e, 0x66, 0x47, 0x45, 0x52, 0x30, 0x4f, 0x73, 0x51,
    0x4d, 0x34, 0x77, 0x71, 0x48, 0x5f, 0x6f, 0x75, 0x77, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

TEST(KafkaParserTest, Basics) {
  Packet packet;
  ParseState parse_state;

  auto produce_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceRequest));
  parse_state = ParseFrame(MessageType::kRequest, &produce_frame_view, &packet);
  EXPECT_EQ(parse_state, ParseState::kSuccess);

  auto short_produce_frame_view = produce_frame_view.substr(0, kMinReqHeaderLength - 1);
  parse_state = ParseFrame(MessageType::kRequest, &short_produce_frame_view, &packet);
  EXPECT_EQ(parse_state, ParseState::kNeedsMoreData);
}

TEST(KafkaParserTest, ParseMultipleRequests) {
  auto produce_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceRequest));
  auto metadata_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kMetaDataRequest));

  Packet expected_message1;
  expected_message1.correlation_id = 4;
  expected_message1.msg = produce_frame_view.substr(kMessageLengthBytes);

  Packet expected_message2;
  expected_message2.correlation_id = 1;
  expected_message2.msg = metadata_frame_view.substr(kMessageLengthBytes);

  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);

  std::deque<Packet> parsed_messages;
  ParseResult result = ParseFramesLoop(MessageType::kRequest, buf, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST(KafkaParserTest, ParseMultipleResponses) {
  auto produce_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceResponse));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kMetaDataResponse));

  Packet expected_message1;
  expected_message1.correlation_id = 4;
  expected_message1.msg = produce_frame_view.substr(kMessageLengthBytes);

  Packet expected_message2;
  expected_message2.correlation_id = 1;
  expected_message2.msg = metadata_frame_view.substr(kMessageLengthBytes);

  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);

  std::deque<Packet> parsed_messages;
  ParseResult result = ParseFramesLoop(MessageType::kResponse, buf, &parsed_messages);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST(KafkaParserTest, ParseIncompleteRequest) {
  auto produce_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceRequest));
  auto truncated_produce_frame = produce_frame_view.substr(0, produce_frame_view.size() - 1);

  std::deque<Packet> parsed_messages;
  ParseResult result =
      ParseFramesLoop(MessageType::kRequest, truncated_produce_frame, &parsed_messages);

  EXPECT_EQ(ParseState::kNeedsMoreData, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

TEST(KafkaParserTest, ParseInvalidInput) {
  std::string msg1("\x00\x00\x18\x00\x03SELECT name FROM users;", 28);

  std::deque<Packet> parsed_messages;
  ParseResult result = ParseFramesLoop(MessageType::kRequest, msg1, &parsed_messages);
  EXPECT_EQ(ParseState::kInvalid, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

TEST(KafkaFindFrameBoundaryTest, FindReqBoundaryAligned) {
  auto produce_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceRequest));
  auto metadata_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kMetaDataRequest));
  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);
  size_t pos = FindFrameBoundary<kafka::Packet>(MessageType::kRequest, buf, 0);
  ASSERT_EQ(pos, 0);
}

TEST(KafkaFindFrameBoundaryTest, FindReqBoundaryUnAligned) {
  auto produce_frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceRequest));
  auto apiversion_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(kMetaDataRequest));
  const std::string buf =
      absl::StrCat(ConstStringView("some garbage"), produce_frame_view, apiversion_frame_view);

  size_t pos = FindFrameBoundary<kafka::Packet>(MessageType::kRequest, buf, 0);
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(buf.substr(pos), absl::StrCat(produce_frame_view, apiversion_frame_view));
}

// TODO(chengruizhe): This test currently fails. Make the check more robust by maybe limiting the
// version numbers.
// TEST(KafkaFindFrameBoundaryTest, FindReqBoundaryWithStartPos) {
//    auto produce_frame_view =
//    CreateStringView<char>(CharArrayStringView<uint8_t>(kProduceRequest)); const std::string_view
//    apiversion_frame_view =
//      CreateStringView<char>(CharArrayStringView<uint8_t>(kAPIVersionRequest));
//    const std::string buf =
//      absl::StrCat(produce_frame_view, apiversion_frame_view);
//
//    size_t pos = FindFrameBoundary<kafka::Packet>(MessageType::kRequest, buf, 1);
//    ASSERT_NE(pos, std::string::npos);
//    EXPECT_EQ(buf.substr(pos), apiversion_frame_view);
//}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
