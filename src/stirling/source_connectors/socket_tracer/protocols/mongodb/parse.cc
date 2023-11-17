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
#include <string>
#include <utility>

#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/decode.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mongodb {

ParseState ParseFrame(message_type_t type, std::string_view* buf, Frame* frame, State* state) {
  if (type != message_type_t::kRequest && type != message_type_t::kResponse) {
    return ParseState::kInvalid;
  }

  BinaryDecoder decoder(*buf);
  if (decoder.BufSize() < mongodb::kHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  // Get the length of the packet. This length contains the size of the field containing the
  // message's length itself.
  PX_ASSIGN_OR(frame->length, decoder.ExtractLEInt<int32_t>(), return ParseState::kInvalid);
  if (decoder.BufSize() < size_t(frame->length - mongodb::kMessageLengthSize)) {
    return ParseState::kNeedsMoreData;
  }

  // Get the Request ID.
  PX_ASSIGN_OR(frame->request_id, decoder.ExtractLEInt<int32_t>(), return ParseState::kInvalid);

  // Get the Response To.
  PX_ASSIGN_OR(frame->response_to, decoder.ExtractLEInt<int32_t>(), return ParseState::kInvalid);

  // Get the message's op code (type).
  PX_ASSIGN_OR(frame->op_code, decoder.ExtractLEInt<int32_t>(), return ParseState::kInvalid);

  // Make sure the op code is a valid type for MongoDB.
  Type frame_type = static_cast<Type>(frame->op_code);
  if (!(frame_type == Type::kOPMsg || frame_type == Type::kOPReply ||
        frame_type == Type::kOPUpdate || frame_type == Type::kOPInsert ||
        frame_type == Type::kReserved || frame_type == Type::kOPQuery ||
        frame_type == Type::kOPGetMore || frame_type == Type::kOPDelete ||
        frame_type == Type::kOPKillCursors || frame_type == Type::kOPCompressed)) {
    return ParseState::kInvalid;
  }

  // Parser will ignore Op Codes that have been deprecated/removed from version 5.0 onwards as well
  // as kOPCompressed and kReserved which are not supported by the parser yet.
  if (frame_type != Type::kOPMsg) {
    buf->remove_prefix(frame->length);
    return ParseState::kIgnored;
  }

  ParseState parse_state = mongodb::ProcessPayload(&decoder, frame);
  if (parse_state == ParseState::kSuccess) {
    *buf = decoder.Buf();

    if (type == message_type_t::kRequest) {
      state->stream_order.push_back(std::pair(frame->request_id, false));
    }
  }

  return parse_state;
}

}  // namespace mongodb

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mongodb::Frame* frame,
                      mongodb::StateWrapper* state) {
  return mongodb::ParseFrame(type, buf, frame, &state->global);
}

template <>
size_t FindFrameBoundary<mongodb::Frame>(message_type_t, std::string_view, size_t,
                                         mongodb::StateWrapper*) {
  // Not implemented.
  return std::string::npos;
}

template <>
mongodb::stream_id_t GetStreamID(mongodb::Frame* frame) {
  if (frame->response_to == 0) {
    return frame->request_id;
  }

  return frame->response_to;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
