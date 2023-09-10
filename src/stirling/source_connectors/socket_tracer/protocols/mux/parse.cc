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

#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

ParseState ParseFullFrame(BinaryDecoder* decoder, Frame* frame) {
  PX_ASSIGN_OR(frame->tag, decoder->ExtractBEInt<uint24_t>(), return ParseState::kInvalid);

  if (frame->tag < 1 || frame->tag > ((1 << 23) - 1)) {
    return ParseState::kInvalid;
  }

  Type frame_type = static_cast<Type>(frame->type);

  if (frame_type == Type::kRerrOld || frame_type == Type::kRerr) {
    PX_ASSIGN_OR(std::string_view why, decoder->ExtractString(frame->MuxBodyLength()),
                 return ParseState::kInvalid);
    frame->why = std::string(why);
    return ParseState::kSuccess;
  }

  if (frame_type == Type::kRinit || frame_type == Type::kTinit) {
    // TODO(ddelnano): Add support for reading Tinit and Rinit compression, tls and other parameters
    return ParseState::kSuccess;
  }

  if (frame_type == Type::kTping || frame_type == Type::kRping) {
    return ParseState::kSuccess;
  }

  if (frame_type == Type::kRdispatch) {
    PX_ASSIGN_OR(frame->reply_status, decoder->ExtractBEInt<uint8_t>(),
                 return ParseState::kInvalid);
  }

  PX_ASSIGN_OR(int16_t num_ctx, decoder->ExtractBEInt<int16_t>(), return ParseState::kInvalid);
  absl::flat_hash_map<std::string, absl::flat_hash_map<std::string, std::string>> context;

  // Parse the key, value context pairs supplied in the Rdispatch/Tdispatch messages.
  // These entries pass distributed tracing context, request deadlines, client id context
  // and retries among others.
  for (int i = 0; i < num_ctx; i++) {
    PX_ASSIGN_OR(size_t ctx_key_len, decoder->ExtractBEInt<int16_t>(), return ParseState::kInvalid);

    PX_ASSIGN_OR(std::string_view ctx_key, decoder->ExtractString(ctx_key_len),
                 return ParseState::kInvalid);

    PX_ASSIGN_OR(size_t ctx_value_len, decoder->ExtractBEInt<int16_t>(),
                 return ParseState::kInvalid);

    absl::flat_hash_map<std::string, std::string> unpacked_value;
    if (ctx_key == "com.twitter.finagle.Deadline") {
      PX_ASSIGN_OR(int64_t timestamp, decoder->ExtractBEInt<int64_t>(),
                   return ParseState::kInvalid);
      PX_ASSIGN_OR(int64_t deadline, decoder->ExtractBEInt<int64_t>(), return ParseState::kInvalid);

      unpacked_value["timestamp"] = std::to_string(timestamp / 1000);
      unpacked_value["deadline"] = std::to_string(deadline / 1000);

    } else if (ctx_key == "com.twitter.finagle.tracing.TraceContext") {
      PX_ASSIGN_OR(int64_t span_id, decoder->ExtractBEInt<int64_t>(), return ParseState::kInvalid);
      PX_ASSIGN_OR(int64_t parent_id, decoder->ExtractBEInt<int64_t>(),
                   return ParseState::kInvalid);
      PX_ASSIGN_OR(int64_t trace_id, decoder->ExtractBEInt<int64_t>(), return ParseState::kInvalid);
      PX_ASSIGN_OR(int64_t flags, decoder->ExtractBEInt<int64_t>(), return ParseState::kInvalid);

      unpacked_value["span id"] = std::to_string(span_id);
      unpacked_value["parent id"] = std::to_string(parent_id);
      unpacked_value["trace id"] = std::to_string(trace_id);
      unpacked_value["flags"] = std::to_string(flags);

    } else if (ctx_key == "com.twitter.finagle.thrift.ClientIdContext") {
      PX_ASSIGN_OR(std::string_view ctx_value, decoder->ExtractString(ctx_value_len),
                   return ParseState::kInvalid);
      unpacked_value["name"] = std::string(ctx_value);

    } else {
      PX_ASSIGN_OR(std::string_view ctx_value, decoder->ExtractString(ctx_value_len),
                   return ParseState::kInvalid);
      unpacked_value["length"] = std::to_string(ctx_value.length());
    }

    frame->InsertContext(ctx_key, std::move(unpacked_value));
  }

  // TODO(ddelnano): Add dest and dtab parsing here
  return ParseState::kSuccess;
}

}  // namespace mux

template <>
ParseState ParseFrame(message_type_t, std::string_view* buf, mux::Frame* frame, NoState*) {
  BinaryDecoder decoder(*buf);

  PX_ASSIGN_OR(frame->length, decoder.ExtractBEInt<int32_t>(), return ParseState::kInvalid);
  if (frame->length > buf->length()) {
    return ParseState::kNeedsMoreData;
  }

  PX_ASSIGN_OR(frame->type, decoder.ExtractBEInt<int8_t>(), return ParseState::kInvalid);
  if (!mux::IsMuxType(frame->type)) {
    return ParseState::kInvalid;
  }

  ParseState parse_state = mux::ParseFullFrame(&decoder, frame);
  if (parse_state == ParseState::kSuccess) {
    constexpr int kHeaderLength = 4;
    buf->remove_prefix(frame->length + kHeaderLength);
  }
  return parse_state;
}

template <>
size_t FindFrameBoundary<mux::Frame>(message_type_t, std::string_view, size_t, NoState*) {
  // Not implemented.
  return std::string::npos;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
