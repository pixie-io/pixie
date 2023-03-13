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

#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/parse.h"
#include <utility>
#include <vector>
#include "src/common/base/types.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/decode.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"

namespace px {
namespace stirling {
namespace protocols {
namespace amqp {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::px::operator<<;
#define PX_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PX_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

TEST(AMQPFrameDecoderTest, TestParseBufferShort) {
  std::string_view input = CreateStringView<char>("\x02\x00\x01\x00\x00\x00\x19");
  Frame packet;
  ParseState state = ParseFrame(message_type_t::kRequest, &input, &packet);
  EXPECT_EQ(state, ParseState::kNeedsMoreData);
}

TEST(AMQPFrameDecoderTest, TestInvalidFrameType) {
  std::string_view input = CreateStringView<char>(
      "\x04\x00\x01\x00\x00\x00\x19\x00\x3c\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x0a\x80\x00\x0a\x74\x65\x78\x74\x2f\x70\x6c\x61\x69\x6e\xce");
  Frame packet;
  ParseState state = ParseFrame(message_type_t::kRequest, &input, &packet);
  EXPECT_EQ(state, ParseState::kInvalid);
}

TEST(AMQPFrameDecoderTest, TestInvalidHeartbeat) {
  std::string_view input = CreateStringView<char>("\x08\x00\x00\x00\x00\x00\x05\xce");
  Frame packet;
  ParseState state = ParseFrame(message_type_t::kRequest, &input, &packet);
  EXPECT_EQ(state, ParseState::kInvalid);
}

TEST(AMQPFrameDecoderTest, TestInvalidEndByte) {
  std::string_view input = CreateStringView<char>("\x08\x00\x00\x00\x00\x00\x00\xFF");
  Frame packet;
  ParseState state = ParseFrame(message_type_t::kRequest, &input, &packet);
  EXPECT_EQ(state, ParseState::kInvalid);
}

TEST(AMQPFrameDecoderTest, TestValidFrame) {
  std::string_view input = CreateStringView<char>("\x08\x00\x00\x00\x00\x00\x00\xce");
  Frame packet;
  ParseState state = ParseFrame(message_type_t::kRequest, &input, &packet);
  EXPECT_EQ(state, ParseState::kSuccess);
}

TEST(AMQPFrameDecoderTest, TestFindFrameBoundaryShort) {
  std::string_view input = CreateStringView<char>("\x02\x00\x01\x00\x00\x00\x19");
  Frame packet;
  size_t n_pos = FindFrameBoundary(input, 0);
  EXPECT_EQ(n_pos, std::string::npos);
}

TEST(AMQPFrameDecoderTest, TestFindFrameBoundaryValid) {
  std::string_view input = CreateStringView<char>("\x04\x0a\x08\x00\x00\x00\x00\x00\x00\xce");
  Frame packet;
  size_t n_pos = FindFrameBoundary(input, 0);
  EXPECT_EQ(n_pos, 2);  // 2nd byte of frame is heartbeat
}

}  // namespace amqp
}  // namespace protocols
}  // namespace stirling
}  // namespace px
