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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/parse.h"

#include <arpa/inet.h>
#include <deque>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/types.h"
#include "src/stirling/utils/binary_decoder.h"
#include "src/stirling/utils/parse_state.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

#define PL_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PL_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

// Kafka request/response format: https://kafka.apache.org/protocol.html#protocol_messages
ParseState ParseFrame(MessageType type, std::string_view* buf, Packet* result) {
  DCHECK(type == MessageType::kRequest || type == MessageType::kResponse);

  if (type == MessageType::kRequest && buf->size() < kafka::kMinReqHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  if (type == MessageType::kResponse && buf->size() < kafka::kMinRespHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  BinaryDecoder binary_decoder(*buf);

  PL_ASSIGN_OR_RETURN_INVALID(int32_t packet_length, binary_decoder.ExtractInt<int32_t>());

  if (packet_length < 0) {
    return ParseState::kInvalid;
  }

  // TODO(chengruizhe): Add Length checks for each command x version. Automatic parsing of the
  // kafka doc will help.

  if (type == MessageType::kRequest) {
    PL_ASSIGN_OR_RETURN_INVALID(int16_t request_api_key, binary_decoder.ExtractInt<int16_t>());
    if (!IsValidAPIKey(request_api_key)) {
      return ParseState::kInvalid;
    }

    PL_ASSIGN_OR_RETURN_INVALID(int16_t request_api_version, binary_decoder.ExtractInt<int16_t>());
    if (request_api_version < 0 || request_api_version > kMaxAPIVersion) {
      return ParseState::kInvalid;
    }
    // TODO(chengruizhe): Add length range checks for each api key x version.
  }

  PL_ASSIGN_OR_RETURN_INVALID(int32_t correlation_id, binary_decoder.ExtractInt<int32_t>());
  if (correlation_id < 0) {
    return ParseState::kInvalid;
  }

  // Putting this check at the end, to avoid invalid packet classified as NeedsMoreData.
  if (buf->size() - kMessageLengthBytes < (size_t)packet_length) {
    return ParseState::kNeedsMoreData;
  }

  result->correlation_id = correlation_id;
  result->msg = buf->substr(kMessageLengthBytes, packet_length);
  buf->remove_prefix(kMessageLengthBytes + packet_length);

  return ParseState::kSuccess;
}

#define PL_ASSIGN_OR_RETURN_NPOS(expr, val_or) PL_ASSIGN_OR(expr, val_or, return std::string::npos)

// FindFrameBoundary currently looks for a proper packet length and valid Kafka api key and version.
// A good idea for improvement is to use correlation_id to find a matching req resp pair,
// which gives us high confidence.
size_t FindFrameBoundary(MessageType type, std::string_view buf, size_t start_pos) {
  if (type == MessageType::kResponse) {
    // Kafka response header only contains a correlation_id, too loose for boundary detection.
    // TODO(chengruizhe): Can we keep a state and check for correlation id matching?
    return std::string::npos;
  }

  if (buf.length() < kafka::kMinReqHeaderLength) {
    return std::string::npos;
  }

  for (size_t i = start_pos; i < buf.size() - kMinReqHeaderLength; ++i) {
    std::string_view cur_buf = buf.substr(i);
    BinaryDecoder binary_decoder(cur_buf);

    PL_ASSIGN_OR_RETURN_NPOS(int32_t packet_length, binary_decoder.ExtractInt<int32_t>());

    if (packet_length < 0 || (size_t)packet_length + kMessageLengthBytes > buf.size()) {
      continue;
    }

    PL_ASSIGN_OR_RETURN_NPOS(int16_t request_api_key, binary_decoder.ExtractInt<int16_t>());

    PL_ASSIGN_OR_RETURN_NPOS(int16_t request_api_version, binary_decoder.ExtractInt<int16_t>());

    PL_ASSIGN_OR_RETURN_NPOS(int32_t correlation_id, binary_decoder.ExtractInt<int32_t>());

    if (!IsValidAPIKey(request_api_key)) {
      continue;
    }

    // TODO(chengruizhe): A tighter check is possible here with the available api versions for
    // each protocol.
    if (request_api_version < 0 || request_api_version > kMaxAPIVersion) {
      continue;
    }

    // TODO(chengruizhe): Add Length checks for each api_key x version. Automatic parsing of the
    // kafka doc will help.

    if (correlation_id < 0) {
      continue;
    }

    // TODO(chengruizhe): Check the client_id field.
    return i;
  }

  return std::string::npos;
}

}  // namespace kafka

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, kafka::Packet* packet) {
  return kafka::ParseFrame(type, buf, packet);
}

template <>
size_t FindFrameBoundary<kafka::Packet>(MessageType type, std::string_view buf, size_t start_pos) {
  return kafka::FindFrameBoundary(type, buf, start_pos);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
