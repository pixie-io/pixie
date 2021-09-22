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

#include <absl/container/flat_hash_set.h>
#include <arpa/inet.h>
#include <deque>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/utils/binary_decoder.h"
#include "src/stirling/utils/parse_state.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

#define PL_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PL_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

// Kafka request/response format: https://kafka.apache.org/protocol.html#protocol_messages
ParseState ParseFrame(message_type_t type, std::string_view* buf, Packet* result, State* state) {
  DCHECK(type == message_type_t::kRequest || type == message_type_t::kResponse);

  if (type == message_type_t::kRequest && buf->size() < kafka::kMinReqHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  if (type == message_type_t::kResponse && buf->size() < kafka::kMinRespHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  BinaryDecoder binary_decoder(*buf);

  PL_ASSIGN_OR_RETURN_INVALID(int32_t packet_length, binary_decoder.ExtractInt<int32_t>());

  if (packet_length < 0) {
    return ParseState::kInvalid;
  }

  // TODO(chengruizhe): Add Length checks for each command x version. Automatic parsing of the
  // kafka doc will help.
  APIKey request_api_key;
  int16_t request_api_version;
  if (type == message_type_t::kRequest) {
    PL_ASSIGN_OR_RETURN_INVALID(int16_t request_api_key_int, binary_decoder.ExtractInt<int16_t>());
    if (!IsValidAPIKey(request_api_key_int)) {
      return ParseState::kInvalid;
    }
    request_api_key = static_cast<APIKey>(request_api_key_int);

    PL_ASSIGN_OR_RETURN_INVALID(request_api_version, binary_decoder.ExtractInt<int16_t>());

    if (!IsSupportedAPIVersion(request_api_key, request_api_version)) {
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

  // Update seen_correlation_ids of requests for more robust response frame parsing.
  if (type == message_type_t::kRequest) {
    state->seen_correlation_ids.insert(correlation_id);
  }
  // TODO(chengruizhe): Check that the correlation_id has been seen before for
  //  responses. If not, e.g. request is missing, get into a confused state.

  result->correlation_id = correlation_id;
  result->msg = buf->substr(kMessageLengthBytes, packet_length);
  buf->remove_prefix(kMessageLengthBytes + packet_length);

  return ParseState::kSuccess;
}

#define PL_ASSIGN_OR_RETURN_NPOS(expr, val_or) PL_ASSIGN_OR(expr, val_or, return std::string::npos)

// FindFrameBoundary currently looks for a proper packet length and valid Kafka api key and version
// in requests, and correlation_id that appeared before in responses.
size_t FindFrameBoundary(message_type_t type, std::string_view buf, size_t start_pos,
                         State* state) {
  size_t min_length = type == message_type_t::kRequest ? kMinReqHeaderLength : kMinRespHeaderLength;

  if (buf.length() < min_length) {
    return std::string::npos;
  }

  for (size_t i = start_pos; i < buf.size() - min_length; ++i) {
    std::string_view cur_buf = buf.substr(i);
    BinaryDecoder binary_decoder(cur_buf);

    PL_ASSIGN_OR_RETURN_NPOS(int32_t packet_length, binary_decoder.ExtractInt<int32_t>());

    if (packet_length <= 0 || (size_t)packet_length + kMessageLengthBytes > buf.size() ||
        (size_t)packet_length + kMessageLengthBytes < min_length) {
      continue;
    }

    // Check for valid api_key and api_version in requests.
    if (type == message_type_t::kRequest) {
      PL_ASSIGN_OR_RETURN_NPOS(int16_t request_api_key, binary_decoder.ExtractInt<int16_t>());
      if (!IsValidAPIKey(request_api_key)) {
        continue;
      }

      PL_ASSIGN_OR_RETURN_NPOS(int16_t request_api_version, binary_decoder.ExtractInt<int16_t>());
      if (!IsSupportedAPIVersion(static_cast<APIKey>(request_api_key), request_api_version)) {
        continue;
      }
    }

    PL_ASSIGN_OR_RETURN_NPOS(int32_t correlation_id, binary_decoder.ExtractInt<int32_t>());
    if (correlation_id < 0) {
      continue;
    }

    // Check for seen correlation_id in responses.
    if (type == message_type_t::kResponse) {
      auto it = state->seen_correlation_ids.find(correlation_id);
      if (it == state->seen_correlation_ids.end()) {
        continue;
      }
    }

    // TODO(chengruizhe): Check the client_id field.
    return i;
  }

  return std::string::npos;
}

}  // namespace kafka

template <>
ParseState ParseFrame<kafka::Packet, kafka::StateWrapper>(message_type_t type,
                                                          std::string_view* buf,
                                                          kafka::Packet* packet,
                                                          kafka::StateWrapper* state) {
  return kafka::ParseFrame(type, buf, packet, &state->global);
}

template <>
size_t FindFrameBoundary<kafka::Packet, kafka::StateWrapper>(message_type_t type,
                                                             std::string_view buf, size_t start_pos,
                                                             kafka::StateWrapper* state) {
  return kafka::FindFrameBoundary(type, buf, start_pos, &state->global);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
