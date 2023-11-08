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

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/base/macros.h>
#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/utils/parse_state.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {

// All protocol Frames should derive off this base definition, which includes standard fields.
struct FrameBase {
  uint64_t timestamp_ns = 0;

  virtual ~FrameBase() = default;

  // ByteSize() is used as part of Cleanup(); used to determine how much memory a tracker is using.
  virtual size_t ByteSize() const = 0;

  // Returns a string describing this object. Make it virtual so some subclasses can omit impl.
  virtual std::string ToString() const {
    return absl::Substitute("timestamp_ns=$0 byte_size=$1", timestamp_ns, ByteSize());
  }
};

struct StartEndPos {
  // Start position is the location of the first byte of the frame.
  size_t start = 0;
  // End position is the location of the last byte of the frame.
  // Unlike STL container end, this is not 1 byte passed the end.
  size_t end = 0;
};

inline bool operator==(const StartEndPos& lhs, const StartEndPos& rhs) {
  return lhs.start == rhs.start && lhs.end == rhs.end;
}

// A ParseResult returns a vector of parsed frames, and also some position markers.
template <typename TKey>
struct ParseResult {
  // Positions of frame start and end positions in the source buffer.
  absl::flat_hash_map<TKey, std::vector<StartEndPos>> frame_positions;
  // Position of where parsing ended consuming the source buffer.
  // This is total bytes successfully consumed.
  size_t end_position;
  // State of the last attempted frame parse.
  ParseState state = ParseState::kInvalid;
  // Number of invalid frames that were discarded.
  int invalid_frames;
  // Total number of bytes parsed into valid frames.
  size_t frame_bytes;
};

/**
 * Parses internal data buffer (see Append()) for frames, and writes resultant
 * parsed frames into the provided frames container.
 *
 * This is a templated function. The caller must provide the type of frame to parsed (e.g.
 * http::Message), and must ensure that the corresponding ParseFrame() function with the desired
 * frame type is implemented.
 *
 * @param type The Type of frames to parse.
 * @param frames The container to which newly parsed frames are added.
 * @param resync If set to true, Parse will first search for the next frame boundary (even
 * if it is currently at a valid frame boundary).
 *
 * @return ParseResult with locations where parseable frames were found in the source buffer.
 */
template <typename TKey, typename TFrameType, typename TStateType = NoState>
ParseResult<TKey> ParseFrames(message_type_t type, DataStreamBuffer* data_stream_buffer,
                              absl::flat_hash_map<TKey, std::deque<TFrameType>>* frames,
                              bool resync = false, TStateType* state = nullptr) {
  std::string_view buf = data_stream_buffer->Head();

  size_t start_pos = 0;
  if (resync) {
    VLOG(2) << "Finding next frame boundary";
    // Since we've been asked to resync, we search from byte 1 to find a new boundary.
    // Don't want to stay at the same position.
    constexpr int kStartPos = 1;
    start_pos = FindFrameBoundary<TFrameType, TStateType>(type, buf, kStartPos, state);

    // Couldn't find a boundary, so stay where we are.
    // Chances are we won't be able to parse, but we have no other option.
    if (start_pos == std::string::npos) {
      start_pos = 0;
    }

    VLOG(1) << absl::Substitute("Removing $0", start_pos);
    buf.remove_prefix(start_pos);
  }

  // Maintain a map of previous sizes.
  absl::flat_hash_map<TKey, size_t> prev_sizes;
  for (const auto& [stream_id, deque] : *frames) {
    prev_sizes[stream_id] = deque.size();
  }

  // Parse and append new frames to the map of stream ID to deque of frames
  ParseResult<TKey> result = ParseFramesLoop(type, buf, frames, state);

  // Compute the number of newly parsed frames for each stream
  size_t total_new_frames = 0;
  for (const auto& [stream_id, positions] : result.frame_positions) {
    total_new_frames += positions.size();
    if (prev_sizes.find(stream_id) != prev_sizes.end()) {
      total_new_frames -= prev_sizes[stream_id];
    }
  }
  VLOG(1) << absl::Substitute("Parsed $0 new frames", total_new_frames);

  // Match timestamps with the parsed frames.
  for (auto& [stream_id, positions] : result.frame_positions) {
    size_t offset = prev_sizes[stream_id];  // Retrieve the initial offset for this stream_id

    for (auto& f : positions) {
      f.start += start_pos;
      f.end += start_pos;

      // Retrieve the message using the current offset
      auto& msg = (*frames)[stream_id][offset];
      offset++;
      StatusOr<uint64_t> timestamp_ns_status =
          data_stream_buffer->GetTimestamp(data_stream_buffer->position() + f.end);
      LOG_IF(ERROR, !timestamp_ns_status.ok()) << timestamp_ns_status.ToString();
      msg.timestamp_ns = timestamp_ns_status.ValueOr(0);
    }
  }
  result.end_position += start_pos;
  return result;
}

/**
 * Calls ParseFrame() repeatedly on a contiguous stream of raw bytes.
 * Places parsed frames into the provided frames container.
 *
 * Note: This is a helper function for EventParser::ParseFrames().
 * It is left public for now because it is used heavily by tests.
 *
 * @param type The Type of frames to parse.
 * @param buf The raw bytes to parse
 * @param frames The output where the parsed frames will be placed.
 *
 * @return ParseResult with locations where parseable frames were found in the source buffer.
 */
// TODO(oazizi): Convert tests to use ParseFrames() instead of ParseFramesLoop().
template <typename TKey, typename TFrameType, typename TStateType = NoState>
ParseResult<TKey> ParseFramesLoop(message_type_t type, std::string_view buf,
                                  absl::flat_hash_map<TKey, std::deque<TFrameType>>* frames,
                                  TStateType* state = nullptr) {
  absl::flat_hash_map<TKey, std::vector<StartEndPos>> frame_positions;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_processed = 0;
  size_t frame_bytes = 0;
  int invalid_count = 0;

  while (!buf.empty() && s != ParseState::kEOS) {
    TFrameType frame;

    s = ParseFrame(type, &buf, &frame, state);

    bool stop = false;
    bool push = false;
    switch (s) {
      case ParseState::kNeedsMoreData:
        // Can't process any more frames.
        stop = true;
        break;
      case ParseState::kInvalid: {
        // An invalid frame may occur when first parsing a connection, or after a lost event.
        // Attempt to look for next valid frame boundary.
        size_t pos = FindFrameBoundary<TFrameType, TStateType>(type, buf, 1, state);
        if (pos != std::string::npos) {
          DCHECK_NE(pos, 0U);
          buf.remove_prefix(pos);
          stop = false;
          push = false;
        } else {
          stop = true;
          push = false;
        }
        ++invalid_count;
      } break;
      case ParseState::kIgnored:
        // Successful case, but do not record the result.
        stop = false;
        push = false;
        break;
      case ParseState::kEOS:
      case ParseState::kSuccess:
        // Successful cases. Record the result.
        stop = false;
        push = true;
        break;
      default:
        DCHECK(false);
    }

    if (stop) {
      break;
    }

    size_t start_position = bytes_processed;
    bytes_processed = (buf_size - buf.size());
    size_t end_position = bytes_processed - 1;

    if (push) {
      // GetStreamID returns 0 by default if not implemented in protocol.
      TKey key = GetStreamID<TKey, TFrameType>(&frame);
      frame_positions[key].push_back({start_position, end_position});
      (*frames)[key].push_back(std::move(frame));
      frame_bytes += (end_position - start_position) + 1;
    }
  }
  return ParseResult<TKey>{std::move(frame_positions), bytes_processed, s, invalid_count,
                           frame_bytes};
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
