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

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/parse.h"

#include <arpa/inet.h>
#include <deque>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"
#include "src/stirling/utils/parse_state.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

ParseState ParseFrame(message_type_t type, std::string_view* buf, Packet* result) {
  if (type != message_type_t::kRequest && type != message_type_t::kResponse) {
    return ParseState::kInvalid;
  }

  if (buf->size() < kPacketHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  result->sequence_id = static_cast<uint8_t>((*buf)[3]);

  int packet_length = utils::LEndianBytesToInt<int, kPayloadLengthLength>(*buf);

  // TODO(oazizi): Is pre-checking requests here a good idea? Somewhat out of place.
  // Better fit for stitcher (when analyzing structure of packet bodies).
  if (type == message_type_t::kRequest) {
    if (buf->size() < kPacketHeaderLength + 1) {
      return ParseState::kInvalid;
    }
    uint8_t command = (*buf)[kPacketHeaderLength];
    if (!IsValidCommand(command)) {
      return ParseState::kInvalid;
    }

    // We can constrain the expected lengths, by command type.
    NumberRange length_range = mysql::kMySQLCommandLengths[command];
    if (packet_length < length_range.min || packet_length > length_range.max) {
      return ParseState::kInvalid;
    }
  }

  ssize_t buffer_length = buf->length();
  if (buffer_length < kPacketHeaderLength + packet_length) {
    return ParseState::kNeedsMoreData;
  }

  result->msg = buf->substr(kPacketHeaderLength, packet_length);
  buf->remove_prefix(kPacketHeaderLength + packet_length);

  return ParseState::kSuccess;
}

size_t FindFrameBoundary(message_type_t type, std::string_view buf, size_t start_pos) {
  if (buf.length() < mysql::kPacketHeaderLength) {
    return std::string::npos;
  }

  if (type == message_type_t::kResponse) {
    // No real search implemented for responses.
    // TODO(oazizi): Is there something we can implement here?
    return std::string::npos;
  }

  // Need at least kPacketHeaderLength bytes + 1 command byte in buf.
  for (size_t i = start_pos; i < buf.size() - mysql::kPacketHeaderLength; ++i) {
    std::string_view cur_buf = buf.substr(i);
    int packet_length = utils::LEndianBytesToInt<int, mysql::kPayloadLengthLength>(cur_buf);
    uint8_t sequence_id = static_cast<uint8_t>(cur_buf[3]);
    auto command_byte = magic_enum::enum_cast<mysql::Command>(cur_buf[mysql::kPacketHeaderLength]);

    // Requests must have sequence id of 0.
    if (sequence_id != 0) {
      continue;
    }

    // If the command byte doesn't decode to a valid command, then this can't a message boundary.
    if (!command_byte.has_value()) {
      continue;
    }

    // We can constrain the expected lengths, by command type.
    auto length_range = mysql::kMySQLCommandLengths[static_cast<int>(command_byte.value())];
    if (packet_length < length_range.min || packet_length > length_range.max) {
      continue;
    }

    return i;
  }

  return std::string::npos;
}

}  // namespace mysql

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mysql::Packet* result,
                      mysql::StateWrapper* /*state*/) {
  return mysql::ParseFrame(type, buf, result);
}

template <>
size_t FindFrameBoundary<mysql::Packet>(message_type_t type, std::string_view buf, size_t start_pos,
                                        mysql::StateWrapper* /*state*/) {
  return mysql::FindFrameBoundary(type, buf, start_pos);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
