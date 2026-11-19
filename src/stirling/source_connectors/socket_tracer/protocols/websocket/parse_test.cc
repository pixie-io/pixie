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

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/parse.h"

#include <string>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace websocket {

constexpr uint8_t kUnmaskedText[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
constexpr uint8_t kMaskedText[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d,
                                   0x7f, 0x9f, 0x4d, 0x51, 0x58};
constexpr uint8_t kFragmentedText[] = {0x01, 0x03, 0x48, 0x65, 0x6c, 0x80, 0x02, 0x6c, 0x6f};
constexpr uint8_t kUnmaskedPing[] = {0x89, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
constexpr uint8_t kMaskedPong[] = {0x8a, 0x85, 0x37, 0xfa, 0x21, 0x3d,
                                   0x7f, 0x9f, 0x4d, 0x51, 0x58};
constexpr uint8_t kCompressedText[] = {0xc1, 0x07, 0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00};
constexpr uint8_t kReservedOpcode[] = {0x83, 0x00};
constexpr uint8_t kFragmentedPing[] = {0x09, 0x00};
constexpr uint8_t kOversizedClose[] = {0x88, 0x7e, 0x00, 0x7e};

template <size_t N>
std::string Bytes(const uint8_t (&arr)[N]) {
  return std::string(CreateCharArrayView<char>(arr));
}

std::string Frame16(uint16_t len) {
  std::string frame = {'\x82', '\x7e', static_cast<char>(len >> 8), static_cast<char>(len & 0xff)};
  return frame + std::string(len, 'x');
}

std::string Frame64(uint64_t len) {
  std::string frame = {'\x82', '\x7f'};
  for (int shift = 56; shift >= 0; shift -= 8) {
    frame.push_back(static_cast<char>((len >> shift) & 0xff));
  }
  return frame + std::string(len, 'x');
}

class WebSocketParseTest : public ::testing::Test {
 protected:
  ParseState Parse(std::string_view* buf) {
    return ParseFrame(message_type_t::kRequest, buf, &frame_, &state_);
  }

  Frame frame_;
  StateWrapper state_;
};

TEST_F(WebSocketParseTest, UnmaskedTextFrame) {
  std::string_view buf = CreateCharArrayView<char>(kUnmaskedText);

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_TRUE(frame_.fin);
  EXPECT_EQ(frame_.opcode, Opcode::kText);
  EXPECT_FALSE(frame_.masked);
  EXPECT_EQ(frame_.payload_length, 5U);
  EXPECT_EQ(frame_.payload, "Hello");
  EXPECT_TRUE(buf.empty());
}

TEST_F(WebSocketParseTest, MaskedTextFrameIsUnmasked) {
  std::string_view buf = CreateCharArrayView<char>(kMaskedText);

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_TRUE(frame_.masked);
  EXPECT_EQ(frame_.opcode, Opcode::kText);
  EXPECT_EQ(frame_.payload, "Hello");
  EXPECT_TRUE(buf.empty());
}

TEST_F(WebSocketParseTest, FragmentedTextFrames) {
  std::string_view buf = CreateCharArrayView<char>(kFragmentedText);

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_FALSE(frame_.fin);
  EXPECT_EQ(frame_.opcode, Opcode::kText);
  EXPECT_EQ(frame_.payload, "Hel");

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_TRUE(frame_.fin);
  EXPECT_EQ(frame_.opcode, Opcode::kContinuation);
  EXPECT_EQ(frame_.payload, "lo");
  EXPECT_TRUE(buf.empty());
}

TEST_F(WebSocketParseTest, ControlFrames) {
  std::string_view ping = CreateCharArrayView<char>(kUnmaskedPing);
  ASSERT_EQ(Parse(&ping), ParseState::kSuccess);
  EXPECT_EQ(frame_.opcode, Opcode::kPing);
  EXPECT_EQ(frame_.payload, "Hello");

  std::string_view pong = CreateCharArrayView<char>(kMaskedPong);
  ASSERT_EQ(Parse(&pong), ParseState::kSuccess);
  EXPECT_EQ(frame_.opcode, Opcode::kPong);
  EXPECT_EQ(frame_.payload, "Hello");
}

TEST_F(WebSocketParseTest, CompressedFrameKeepsRsvBits) {
  std::string_view buf = CreateCharArrayView<char>(kCompressedText);

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_EQ(frame_.rsv, 0x4);
  EXPECT_EQ(frame_.opcode, Opcode::kText);
  EXPECT_EQ(frame_.payload, Bytes(kCompressedText).substr(2));
}

TEST_F(WebSocketParseTest, ExtendedPayloadLengths) {
  std::string frame16 = Frame16(256);
  std::string_view buf16 = frame16;
  ASSERT_EQ(Parse(&buf16), ParseState::kSuccess);
  EXPECT_EQ(frame_.opcode, Opcode::kBinary);
  EXPECT_EQ(frame_.payload_length, 256U);
  EXPECT_EQ(frame_.payload.size(), 256U);
  EXPECT_TRUE(buf16.empty());

  std::string frame64 = Frame64(65536);
  std::string_view buf64 = frame64;
  ASSERT_EQ(Parse(&buf64), ParseState::kSuccess);
  EXPECT_EQ(frame_.payload_length, 65536U);
  EXPECT_EQ(frame_.payload.size(), 65536U);
  EXPECT_TRUE(buf64.empty());
}

TEST_F(WebSocketParseTest, BackToBackFrames) {
  std::string data = Bytes(kMaskedText) + Bytes(kUnmaskedPing);
  std::string_view buf = data;

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_EQ(frame_.opcode, Opcode::kText);
  EXPECT_EQ(buf.size(), sizeof(kUnmaskedPing));

  ASSERT_EQ(Parse(&buf), ParseState::kSuccess);
  EXPECT_EQ(frame_.opcode, Opcode::kPing);
  EXPECT_TRUE(buf.empty());
}

TEST_F(WebSocketParseTest, IncompleteFramesNeedMoreData) {
  std::string full = Bytes(kMaskedText);
  for (size_t len : {1UL, 2UL, 5UL, full.size() - 1}) {
    std::string_view buf(full.data(), len);
    EXPECT_EQ(Parse(&buf), ParseState::kNeedsMoreData) << len;
    EXPECT_EQ(buf.size(), len) << len;
  }

  std::string frame64 = Frame64(65536);
  std::string_view header_only(frame64.data(), 6);
  EXPECT_EQ(Parse(&header_only), ParseState::kNeedsMoreData);
}

TEST_F(WebSocketParseTest, InvalidFrames) {
  std::string_view reserved = CreateCharArrayView<char>(kReservedOpcode);
  EXPECT_EQ(Parse(&reserved), ParseState::kInvalid);

  std::string_view fragmented_ping = CreateCharArrayView<char>(kFragmentedPing);
  EXPECT_EQ(Parse(&fragmented_ping), ParseState::kInvalid);

  std::string_view oversized_close = CreateCharArrayView<char>(kOversizedClose);
  EXPECT_EQ(Parse(&oversized_close), ParseState::kInvalid);

  std::string msb_set = {'\x82', '\x7f', '\x80', 0, 0, 0, 0, 0, 0, 0};
  std::string_view msb_view = msb_set;
  EXPECT_EQ(Parse(&msb_view), ParseState::kInvalid);
}

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
