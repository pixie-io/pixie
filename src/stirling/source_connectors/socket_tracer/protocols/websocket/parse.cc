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

#include <algorithm>
#include <optional>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/k8s_exec.h"
#include "src/stirling/utils/binary_decoder.h"

DEFINE_uint32(websocket_payload_limit_bytes,
              gflags::Uint32FromEnv("PX_STIRLING_WEBSOCKET_PAYLOAD_LIMIT_BYTES", 1024),
              "The amount of a WebSocket frame payload that will be returned on a parse");

namespace px {
namespace stirling {
namespace protocols {

namespace websocket {
namespace {

constexpr uint8_t kFinBit = 0x80;
constexpr uint8_t kRsvBits = 0x70;
constexpr uint8_t kUnsupportedRsvBits = 0x30;
constexpr uint8_t kRsvShift = 4;
constexpr uint8_t kOpcodeBits = 0x0f;
constexpr uint8_t kMaskBit = 0x80;
constexpr uint8_t kPayloadLengthBits = 0x7f;
constexpr uint8_t kPayloadLength16 = 126;
constexpr uint8_t kPayloadLength64 = 127;
constexpr uint64_t kPayloadLength64MSB = 1ULL << 63;
constexpr uint64_t kMaxControlPayloadLength = 125;
constexpr size_t kMaskingKeySize = 4;

struct Header {
  bool fin = false;
  uint8_t rsv = 0;
  Opcode opcode = Opcode::kContinuation;
  bool masked = false;
  uint64_t payload_length = 0;
  std::string_view masking_key;
};

ParseState ParseHeader(BinaryDecoder* decoder, Header* header) {
  PX_ASSIGN_OR(uint8_t b0, decoder->ExtractBEInt<uint8_t>(), return ParseState::kNeedsMoreData);
  PX_ASSIGN_OR(uint8_t b1, decoder->ExtractBEInt<uint8_t>(), return ParseState::kNeedsMoreData);

  std::optional<Opcode> opcode = magic_enum::enum_cast<Opcode>(b0 & kOpcodeBits);
  if (!opcode.has_value() || (b0 & kUnsupportedRsvBits)) {
    return ParseState::kInvalid;
  }
  header->fin = b0 & kFinBit;
  const bool control = IsControl(opcode.value());
  if (control && !header->fin) {
    return ParseState::kInvalid;
  }

  uint64_t payload_length = b1 & kPayloadLengthBits;
  if (payload_length == kPayloadLength16) {
    PX_ASSIGN_OR(payload_length, decoder->ExtractBEInt<uint16_t>(),
                 return ParseState::kNeedsMoreData);
  } else if (payload_length == kPayloadLength64) {
    PX_ASSIGN_OR(payload_length, decoder->ExtractBEInt<uint64_t>(),
                 return ParseState::kNeedsMoreData);
    if (payload_length & kPayloadLength64MSB) {
      return ParseState::kInvalid;
    }
  }
  if (control && payload_length > kMaxControlPayloadLength) {
    return ParseState::kInvalid;
  }

  header->masked = b1 & kMaskBit;
  if (header->masked) {
    PX_ASSIGN_OR(header->masking_key, decoder->ExtractString(kMaskingKeySize),
                 return ParseState::kNeedsMoreData);
  }
  header->rsv = (b0 & kRsvBits) >> kRsvShift;
  header->opcode = opcode.value();
  header->payload_length = payload_length;
  return ParseState::kSuccess;
}

DirectionState* Direction(message_type_t type, StateWrapper* state) {
  if (state == nullptr) {
    return nullptr;
  }
  return type == message_type_t::kRequest ? &state->send : &state->recv;
}

ParseState CheckHeaderAt(message_type_t type, std::string_view buf, uint64_t* frame_size) {
  BinaryDecoder decoder(buf);
  Header header;
  ParseState s = ParseHeader(&decoder, &header);
  if (s != ParseState::kSuccess) {
    return s;
  }
  if (header.masked != (type == message_type_t::kRequest)) {
    return ParseState::kInvalid;
  }
  *frame_size = buf.size() - decoder.BufSize() + header.payload_length;
  return ParseState::kSuccess;
}

bool IsFrameBoundary(message_type_t type, std::string_view buf, size_t pos) {
  uint64_t frame_size = 0;
  if (CheckHeaderAt(type, buf.substr(pos), &frame_size) != ParseState::kSuccess) {
    return false;
  }
  const size_t remaining = buf.size() - pos;
  if (frame_size >= remaining) {
    return frame_size == remaining;
  }
  uint64_t next_frame_size = 0;
  return CheckHeaderAt(type, buf.substr(pos + frame_size), &next_frame_size) !=
         ParseState::kInvalid;
}

}  // namespace

size_t PayloadLimitBytes(const StateWrapper* state) {
  if (state != nullptr && IsK8sChannelProtocol(state->global.subprotocol)) {
    return kK8sChannelPayloadLimitBytes;
  }
  return FLAGS_websocket_payload_limit_bytes;
}

}  // namespace websocket

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, websocket::Frame* frame,
                      websocket::StateWrapper* state) {
  websocket::DirectionState* direction = websocket::Direction(type, state);
  if (direction != nullptr && direction->skip > 0) {
    const size_t skipped = std::min<uint64_t>(direction->skip, buf->size());
    buf->remove_prefix(skipped);
    direction->skip -= skipped;
    return ParseState::kIgnored;
  }

  BinaryDecoder decoder(*buf);
  websocket::Header header;
  ParseState s = websocket::ParseHeader(&decoder, &header);
  if (s != ParseState::kSuccess) {
    return s;
  }

  const uint64_t captured =
      direction == nullptr
          ? header.payload_length
          : std::min<uint64_t>(header.payload_length, websocket::PayloadLimitBytes(state));
  PX_ASSIGN_OR(std::string_view payload, decoder.ExtractString(captured),
               return ParseState::kNeedsMoreData);
  const uint64_t uncaptured = header.payload_length - captured;
  const size_t consumed = std::min<uint64_t>(uncaptured, decoder.BufSize());

  frame->fin = header.fin;
  frame->rsv = header.rsv;
  frame->opcode = header.opcode;
  frame->masked = header.masked;
  frame->payload_length = header.payload_length;
  frame->payload.assign(payload);
  if (header.masked) {
    for (size_t i = 0; i < frame->payload.size(); ++i) {
      frame->payload[i] ^= header.masking_key[i % websocket::kMaskingKeySize];
    }
  }

  buf->remove_prefix(buf->size() - decoder.BufSize() + consumed);
  if (direction != nullptr) {
    direction->skip = uncaptured - consumed;
  }
  return ParseState::kSuccess;
}

template <>
size_t FindFrameBoundary<websocket::Frame>(message_type_t type, std::string_view buf,
                                           size_t start_pos, websocket::StateWrapper* state) {
  websocket::DirectionState* direction = websocket::Direction(type, state);
  if (direction != nullptr) {
    direction->skip = 0;
  }
  for (size_t pos = start_pos; pos < buf.size(); ++pos) {
    if (websocket::IsFrameBoundary(type, buf, pos)) {
      return pos;
    }
  }
  return std::string_view::npos;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
