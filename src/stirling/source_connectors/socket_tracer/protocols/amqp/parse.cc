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

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types.h"
#include "src/stirling/utils/binary_decoder.h"
#include "types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace amqp {

namespace {

StatusOr<Message::type> ParseType(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(uint8_t type, decoder->ExtractInt<uint8_t>());

  if (!(type >= 1 && type <= 4)) {
    return error::InvalidArgument(
        "AMQP message type is parsed as $0, but only [1,4] are defined in protocol specification.",
        type);
  }

  return static_cast<Message::type>(type);
}

StatusOr<uint16_t> ParseChannel(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(uint16_t channel, decoder->ExtractInt<uint16_t>());

  return channel;
}

StatusOr<uint64_t> ParseLength(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(uint64_t size, decoder->ExtractInt<uint64_t>());

  return size;
}

StatusOr<std::string_view> ParsePayload(BinaryDecoder* decoder) {
  // payload parsing
  return std::string_view();
}

Status ParseMessage(message_type_t type, BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(msg->message_type, ParseType(decoder));
  PL_ASSIGN_OR_RETURN(msg->message_channel, ParseChannel(decoder));
  PL_ASSIGN_OR_RETURN(msg->message_length, ParseLength(decoder));
  PL_ASSIGN_OR_RETURN(msg->message_body, ParsePayload(decoder));

  // This is needed for GCC build.
  return Status::OK();
}

}  // namespace

size_t FindMessageBoundary(std::string_view buf, size_t start_pos) {
  for (; start_pos < buf.size(); ++start_pos) {
    const char type_marker = buf[start_pos];
    if (type_marker == Message::kFrameEnd) {
      return start_pos + 1;
    }
  }
  return std::string_view::npos;
}

ParseState ParseMessage(message_type_t type, std::string_view* buf, Message* msg) {
  BinaryDecoder decoder(*buf);

  auto status = ParseMessage(type, &decoder, msg);

  if (!status.ok()) {
    return TranslateStatus(status);
  }

  *buf = decoder.Buf();

  return ParseState::kSuccess;
}

}  // namespace amqp

template <>
size_t FindFrameBoundary<amqp::Message>(message_type_t /*type*/, std::string_view buf,
                                        size_t start_pos, NoState* /*state*/) {
  return amqp::FindMessageBoundary(buf, start_pos);
}

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, redis::Message* msg,
                      NoState* /*state*/) {
  return amqp::ParseMessage(type, buf, msg);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
