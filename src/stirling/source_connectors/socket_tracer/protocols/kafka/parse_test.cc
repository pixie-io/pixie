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

#include <absl/container/flat_hash_map.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>

#include "src/common/base/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/test_data.h"

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

TEST(KafkaParserTest, Basics) {
  Packet packet;
  ParseState parse_state;
  State state;

  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceRequest));
  parse_state = ParseFrame(message_type_t::kRequest, &produce_frame_view, &packet, &state);
  EXPECT_EQ(parse_state, ParseState::kSuccess);
  EXPECT_TRUE(state.seen_correlation_ids.contains(4));

  state = {};
  auto short_produce_frame_view = produce_frame_view.substr(0, kMinReqPacketLength - 1);
  parse_state = ParseFrame(message_type_t::kRequest, &short_produce_frame_view, &packet, &state);
  EXPECT_EQ(parse_state, ParseState::kNeedsMoreData);
  EXPECT_TRUE(state.seen_correlation_ids.empty());
}

TEST(KafkaParserTest, ParseMultipleRequests) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceRequest));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kMetaDataRequest));

  Packet expected_message1;
  expected_message1.correlation_id = 4;
  expected_message1.msg = produce_frame_view.substr(kMessageLengthBytes);

  Packet expected_message2;
  expected_message2.correlation_id = 1;
  expected_message2.msg = metadata_frame_view.substr(kMessageLengthBytes);

  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);

  absl::flat_hash_map<correlation_id_t, std::deque<Packet>> parsed_messages;
  StateWrapper state;
  ParseResult<correlation_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, buf, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_message1, expected_message2));
  EXPECT_TRUE(state.global.seen_correlation_ids.contains(expected_message1.correlation_id));
  EXPECT_TRUE(state.global.seen_correlation_ids.contains(expected_message2.correlation_id));
}

TEST(KafkaParserTest, ParseMultipleResponses) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceResponse));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kMetaDataResponse));

  Packet expected_message1;
  expected_message1.correlation_id = 4;
  expected_message1.msg = produce_frame_view.substr(kMessageLengthBytes);

  Packet expected_message2;
  expected_message2.correlation_id = 1;
  expected_message2.msg = metadata_frame_view.substr(kMessageLengthBytes);

  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);

  absl::flat_hash_map<correlation_id_t, std::deque<Packet>> parsed_messages;
  StateWrapper state{.global = {{1, 4}}, .send = {}, .recv = {}};
  ParseResult<correlation_id_t> result =
      ParseFramesLoop(message_type_t::kResponse, buf, &parsed_messages, &state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_message1, expected_message2));
}

TEST(KafkaParserTest, ParseIncompleteRequest) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceRequest));
  auto truncated_produce_frame = produce_frame_view.substr(0, produce_frame_view.size() - 1);

  absl::flat_hash_map<correlation_id_t, std::deque<Packet>> parsed_messages;
  StateWrapper state;
  ParseResult<correlation_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, truncated_produce_frame, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kNeedsMoreData, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre());
  EXPECT_TRUE(state.global.seen_correlation_ids.empty());
}

TEST(KafkaParserTest, ParseInvalidInput) {
  std::string msg1("\x00\x00\x18\x00\x03SELECT name FROM users;", 28);

  absl::flat_hash_map<correlation_id_t, std::deque<Packet>> parsed_messages;
  StateWrapper state;
  ParseResult<correlation_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, msg1, &parsed_messages, &state);
  EXPECT_EQ(ParseState::kInvalid, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
  EXPECT_TRUE(state.global.seen_correlation_ids.empty());
}

TEST(KafkaFindFrameBoundaryTest, FindReqBoundaryAligned) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceRequest));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kMetaDataRequest));
  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);
  State state;

  size_t pos = FindFrameBoundary(message_type_t::kRequest, buf, 0, &state);
  ASSERT_EQ(pos, 0);
}

TEST(KafkaFindFrameBoundaryTest, FindReqBoundaryUnAligned) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceRequest));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kMetaDataRequest));
  const std::string buf =
      absl::StrCat(ConstStringView("some garbage"), produce_frame_view, metadata_frame_view);
  State state;

  size_t pos = FindFrameBoundary(message_type_t::kRequest, buf, 0, &state);
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(buf.substr(pos), absl::StrCat(produce_frame_view, metadata_frame_view));
}

TEST(KafkaFindFrameBoundaryTest, FindRespBoundaryAligned) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceResponse));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kMetaDataResponse));

  const std::string buf = absl::StrCat(produce_frame_view, metadata_frame_view);
  State state{.seen_correlation_ids = {1, 4}};

  size_t pos = FindFrameBoundary(message_type_t::kResponse, buf, 0, &state);
  ASSERT_EQ(pos, 0);
}

TEST(KafkaFindFrameBoundaryTest, FindRespBoundaryUnAligned) {
  auto produce_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceResponse));
  auto metadata_frame_view =
      CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kMetaDataResponse));

  const std::string buf =
      absl::StrCat(ConstStringView("some garbage"), produce_frame_view, metadata_frame_view);
  State state{.seen_correlation_ids = {1, 4}};

  size_t pos = FindFrameBoundary(message_type_t::kResponse, buf, 0, &state);
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(buf.substr(pos), absl::StrCat(produce_frame_view, metadata_frame_view));

  // If the correlation_id of produce response (i.e. 4) is not seen, this should skip over it.
  state = {.seen_correlation_ids = {1}};
  pos = FindFrameBoundary(message_type_t::kResponse, buf, 0, &state);
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(buf.substr(pos), metadata_frame_view);
}

// TODO(chengruizhe): This test currently fails. Make the check more robust by maybe limiting the
// version numbers.
// TEST(KafkaFindFrameBoundaryTest, FindReqBoundaryWithStartPos) {
//    auto produce_frame_view =
//    CreateStringView<char>(CharArrayStringView<uint8_t>(testdata::kProduceRequest)); const
//    std::string_view apiversion_frame_view =
//      CreateStringView<char>(CharArrayStringView<uint8_t>(kAPIVersionRequest));
//    const std::string buf =
//      absl::StrCat(produce_frame_view, apiversion_frame_view);
//
//    size_t pos = FindFrameBoundary(message_type_t::kRequest, buf, 1);
//    ASSERT_NE(pos, std::string::npos);
//    EXPECT_EQ(buf.substr(pos), apiversion_frame_view);
//}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
