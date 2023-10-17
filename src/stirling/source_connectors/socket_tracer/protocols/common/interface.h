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

#pragma once

#include <absl/container/flat_hash_map.h>
#include <deque>
#include <variant>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"
#include "src/stirling/utils/parse_state.h"

#define CTX_DCHECK(x) DCHECK(x) << "[" << __FILE__ << "]"
#define CTX_DCHECK_EQ(x, y) DCHECK_EQ(x, y) << "[" << __FILE__ << "]"
#define CTX_DCHECK_NE(x, y) DCHECK_NE(x, y) << "[" << __FILE__ << "]"
#define CTX_DCHECK_GE(x, y) DCHECK_GE(x, y) << "[" << __FILE__ << "]"
#define CTX_DCHECK_GT(x, y) DCHECK_GT(x, y) << "[" << __FILE__ << "]"
#define CTX_DCHECK_LE(x, y) DCHECK_LE(x, y) << "[" << __FILE__ << "]"
#define CTX_ECHECK_LT(x, y) ECHECK_LT(x, y) << "[" << __FILE__ << "]"

namespace px {
namespace stirling {
namespace protocols {

/**
 * Struct that should be the return type of ParseFrames() API in protocol pipeline stitchers.
 * @tparam TRecord Record type of the protocol.
 */
template <typename TRecord>
struct RecordsWithErrorCount {
  std::vector<TRecord> records;
  int error_count = 0;
};

// Each protocol should define a struct called defining its protocol traits.
// This ProtocolTraits struct should define the following types:
// - frame_type: This is the low-level frame to which the raw data is parsed.
//               Examples: http::Message, cql::Frame, mysql::Packet
// - state_type: This is state struct that contains any relevant state for the protocol.
//               The state_type must have three members: global, send and recv.
//               A convenience NoState struct is defined for any protocols that have no state.
// - record_type: This is the request response pair, the content of which has been interpreted.
//                This struct will be passed to the SocketTraceConnector to be appended to the
//                appropriate table.
//
// Example for HTTP protocol:
//
// namespace http {
// struct ProtocolTraits {
//   using frame_type = Message;
//   using record_type = Record;
//   using state_type = NoState;
// };
// }
//
// Note that the ProtocolTraits are hooked into the SocketTraceConnector through the
// protocol_transfer_specs.

// A default state implementation, provided for convenience.
// Setting ProtocolTraits::state_type to NoState indicate that there is no state for the protocol.
// As an optimization, the connection tracker understands not to create state object for NoState.
struct NoState {
  std::monostate global;
  std::monostate send;
  std::monostate recv;
};

// NOTE: FindFrameBoundary(), ParseFrame(), and StitchFrames() must be implemented per protocol.

/**
 * Attempt to find the next frame boundary.
 *
 * @tparam TFrameType Message type to search for.
 * @param type request or response.
 * @param buf the buffer in which to search for a frame boundary.
 * @param start_pos A start position from which to search.
 * @return Either the position of a frame start, if found (must be > start_pos),
 * or std::string::npos if no such frame start was found.
 */
template <typename TFrameType, typename TStateType = NoState>
size_t FindFrameBoundary(message_type_t type, std::string_view buf, size_t start_pos,
                         TStateType* state = nullptr);

/**
 * Parses the input string to extract a single frame of the specified protocol.
 *
 * @tparam TFrameType Type of frame to parse.
 * @param type Whether to process frame as a request or response.
 * @param buf The raw data to be parsed. Any processed bytes are removed from the buffer, if parsing
 * succeeded.
 * @param frame The parsed frame if parsing succeeded.
 *
 * @return ParseState Indicates whether the parsing succeeded or not.
 */
template <typename TFrameType, typename TStateType = NoState>
ParseState ParseFrame(message_type_t type, std::string_view* buf, TFrameType* frame,
                      TStateType* state = nullptr);

/**
 * Returns the stream ID of the given frame.
 *
 * @tparam TFrameType Type of frame to parse.
 * @param frame The frame to get the stream ID from.
 * @return The stream ID of the given frame.
 */
template <typename TKey, typename TFrameType>
TKey GetStreamID(TFrameType*) {
  return TKey(0);
}

/**
 * StitchFrames is the entry point of stitcher for all protocols. It loops through the responses,
 * matches them with the corresponding requests, and returns stitched request & response pairs.
 *
 * @param requests: deque of all request messages.
 * @param responses: deque of all response messages.
 * @return A vector of entries to be appended to table store.
 */
template <typename TRecordType, typename TFrameType, typename TStateType>
RecordsWithErrorCount<TRecordType> StitchFrames(std::deque<TFrameType>* requests,
                                                std::deque<TFrameType>* responses,
                                                TStateType* state);

/**
 * For protocols that support streams, we use a map of stream ID to frames.
 *
 * @param requests: map of stream ID to deque of request frames.
 * @param responses: map of stream ID to deque of response frames.
 * @return A vector of entries to be appended to table store.
 */
template <typename TRecordType, typename TKey, typename TFrameType, typename TStateType>
RecordsWithErrorCount<TRecordType> StitchFrames(
    absl::flat_hash_map<TKey, std::deque<TFrameType>>* requests,
    absl::flat_hash_map<TKey, std::deque<TFrameType>>* responses, TStateType* state);

/**
 * The BaseProtocolTraits all ProtocolTraits should inherit from. It provides a default
 * UpdateTimestamps method that applies to most protocols.
 * @tparam TRecord The type of the record for the derived ProtocolTraits.
 */
template <typename TRecord>
struct BaseProtocolTraits {
  using ConvertTimestampsFuncType = const std::function<uint64_t(uint64_t)>&;
  static void ConvertTimestamps(TRecord* record, ConvertTimestampsFuncType func) {
    record->req.timestamp_ns = func(record->req.timestamp_ns);
    record->resp.timestamp_ns = func(record->resp.timestamp_ns);
  }
  enum StreamSupport { NoStream, UseStream };
  // Protocol does not support streams by default. Override this in the derived ProtocolTraits to
  // parse frames into map of stream ID to frames instead of a single deque for all streams.
  static constexpr StreamSupport stream_support = NoStream;
};

}  // namespace protocols
}  // namespace stirling
}  // namespace px
