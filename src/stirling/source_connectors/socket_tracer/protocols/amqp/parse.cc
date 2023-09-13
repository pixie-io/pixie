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
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/decode.h"

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace amqp {

#define PX_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PX_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

// The header frame for AMQP is usually split as
// 0      1         3             7                  size+7 size+8
// +------+---------+-------------+  +------------+  +-----------+
// | type | channel |     size    |  |  payload   |  | frame-end |
// +------+---------+-------------+  +------------+  +-----------+
//  octet   short         long         size octets       octet

// The first octet can be used to quickly determine if the message boundary starts
size_t FindFrameBoundary(std::string_view buf, size_t start_pos) {
  // LOG(DEBUG) << "IN AMQP find frame boundary";
  if (buf.size() < kMinFrameLength) {
    return std::string::npos;
  }

  for (; start_pos < buf.size(); ++start_pos) {
    const AMQPFrameTypes type_marker = static_cast<AMQPFrameTypes>(buf[start_pos]);
    if (type_marker == AMQPFrameTypes::kFrameHeader || type_marker == AMQPFrameTypes::kFrameBody ||
        type_marker == AMQPFrameTypes::kFrameMethod ||
        type_marker == AMQPFrameTypes::kFrameHeartbeat) {
      return start_pos;
    }
  }
  return std::string_view::npos;
}

// Parse the message's type, channel
ParseState ParseFrame(message_type_t type, std::string_view* buf, Frame* packet) {
  CTX_DCHECK(type == message_type_t::kRequest || type == message_type_t::kResponse);
  BinaryDecoder decoder(*buf);
  if (decoder.BufSize() < kMinFrameLength) {
    return ParseState::kNeedsMoreData;
  }

  PX_ASSIGN_OR_RETURN_INVALID(uint8_t frame_type, decoder.ExtractBEInt<uint8_t>());
  AMQPFrameTypes frame_type_header = static_cast<AMQPFrameTypes>(frame_type);
  if (!(frame_type_header == AMQPFrameTypes::kFrameHeader ||
        frame_type_header == AMQPFrameTypes::kFrameBody ||
        frame_type_header == AMQPFrameTypes::kFrameMethod ||
        frame_type_header == AMQPFrameTypes::kFrameHeartbeat)) {
    return ParseState::kInvalid;
  }

  PX_ASSIGN_OR_RETURN_INVALID(uint16_t channel, decoder.ExtractBEInt<uint16_t>());
  PX_ASSIGN_OR_RETURN_INVALID(uint32_t payload_size, decoder.ExtractBEInt<uint32_t>());
  if (frame_type_header == AMQPFrameTypes::kFrameHeartbeat && payload_size != 0) {
    return ParseState::kInvalid;
  }
  packet->frame_type = frame_type;
  packet->channel = channel;
  packet->payload_size = payload_size;
  if (decoder.BufSize() <= payload_size) {
    return ParseState::kNeedsMoreData;
  }

  // Check end byte is valid
  const uint8_t frame_end_loc = decoder.Buf().at(packet->payload_size);
  if (frame_end_loc != kFrameEnd) {
    return ParseState::kInvalid;
  }
  // Remove up till start of message payload
  if (!ProcessPayload(packet, &decoder).ok()) {
    return ParseState::kInvalid;
  }
  packet->full_body_parsed = true;
  PX_ASSIGN_OR_EXIT(uint8_t frame_end_after_parsing, decoder.ExtractChar());
  if (frame_end_after_parsing != kFrameEnd) {
    return ParseState::kInvalid;
  }

  if (frame_end_loc != kFrameEnd) {
    return ParseState::kInvalid;
  }

  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

}  // namespace amqp

template <>
size_t FindFrameBoundary<amqp::Frame>(message_type_t /*type*/, std::string_view buf,
                                      size_t start_pos, NoState* /*state*/) {
  return amqp::FindFrameBoundary(buf, start_pos);
}

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, amqp::Frame* msg,
                      NoState* /*state*/) {
  return amqp::ParseFrame(type, buf, msg);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
