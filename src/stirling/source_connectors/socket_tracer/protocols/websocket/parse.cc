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

#include <optional>
#include <string>

#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {

namespace websocket {
namespace {

constexpr uint8_t kFinBit = 0x80;
constexpr uint8_t kRsvBits = 0x70;
constexpr uint8_t kRsvShift = 4;
constexpr uint8_t kOpcodeBits = 0x0f;
constexpr uint8_t kControlOpcodeBit = 0x08;
constexpr uint8_t kMaskBit = 0x80;
constexpr uint8_t kPayloadLengthBits = 0x7f;
constexpr uint8_t kPayloadLength16 = 126;
constexpr uint8_t kPayloadLength64 = 127;
constexpr uint64_t kPayloadLength64MSB = 1ULL << 63;
constexpr uint64_t kMaxControlPayloadLength = 125;
constexpr size_t kMaskingKeySize = 4;

bool IsControl(Opcode opcode) { return static_cast<uint8_t>(opcode) & kControlOpcodeBit; }

}  // namespace
}  // namespace websocket

template <>
ParseState ParseFrame(message_type_t, std::string_view* buf, websocket::Frame* frame, NoState*) {
  using websocket::Opcode;

  BinaryDecoder decoder(*buf);
  PX_ASSIGN_OR(uint8_t b0, decoder.ExtractBEInt<uint8_t>(), return ParseState::kNeedsMoreData);
  PX_ASSIGN_OR(uint8_t b1, decoder.ExtractBEInt<uint8_t>(), return ParseState::kNeedsMoreData);

  std::optional<Opcode> opcode = magic_enum::enum_cast<Opcode>(b0 & websocket::kOpcodeBits);
  if (!opcode.has_value()) {
    return ParseState::kInvalid;
  }
  const bool fin = b0 & websocket::kFinBit;
  const bool control = websocket::IsControl(opcode.value());
  if (control && !fin) {
    return ParseState::kInvalid;
  }

  uint64_t payload_length = b1 & websocket::kPayloadLengthBits;
  if (payload_length == websocket::kPayloadLength16) {
    PX_ASSIGN_OR(payload_length, decoder.ExtractBEInt<uint16_t>(),
                 return ParseState::kNeedsMoreData);
  } else if (payload_length == websocket::kPayloadLength64) {
    PX_ASSIGN_OR(payload_length, decoder.ExtractBEInt<uint64_t>(),
                 return ParseState::kNeedsMoreData);
    if (payload_length & websocket::kPayloadLength64MSB) {
      return ParseState::kInvalid;
    }
  }
  if (control && payload_length > websocket::kMaxControlPayloadLength) {
    return ParseState::kInvalid;
  }

  const bool masked = b1 & websocket::kMaskBit;
  std::string_view masking_key;
  if (masked) {
    PX_ASSIGN_OR(masking_key, decoder.ExtractString(websocket::kMaskingKeySize),
                 return ParseState::kNeedsMoreData);
  }
  PX_ASSIGN_OR(std::string_view payload, decoder.ExtractString(payload_length),
               return ParseState::kNeedsMoreData);

  frame->fin = fin;
  frame->rsv = (b0 & websocket::kRsvBits) >> websocket::kRsvShift;
  frame->opcode = opcode.value();
  frame->masked = masked;
  frame->payload_length = payload_length;
  frame->payload.assign(payload);
  if (masked) {
    for (size_t i = 0; i < frame->payload.size(); ++i) {
      frame->payload[i] ^= masking_key[i % websocket::kMaskingKeySize];
    }
  }

  buf->remove_prefix(buf->size() - decoder.BufSize());
  return ParseState::kSuccess;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
