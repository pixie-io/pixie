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

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/http2_streams_container.h"

#include <algorithm>

namespace px {
namespace stirling {

namespace {

void EraseExpiredStreams(std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp,
                         absl::flat_hash_map<uint32_t, protocols::http2::Stream>* streams) {
  auto iter = streams->begin();
  while (iter != streams->end()) {
    const auto& stream = iter->second;
    uint64_t timestamp_ns = std::max(stream.send.timestamp_ns, stream.recv.timestamp_ns);
    auto last_activity =
        std::chrono::time_point<std::chrono::steady_clock>(std::chrono::nanoseconds(timestamp_ns));

    if (expiry_timestamp < last_activity) {
      break;
    }
    streams->erase(iter++);
  }
}
}  // namespace

size_t HTTP2StreamsContainer::StreamsSize() const {
  size_t size = 0;
  for (const auto& [id, stream] : streams_) {
    size += stream.ByteSize();
  }
  return size;
}

void HTTP2StreamsContainer::Cleanup(
    size_t size_limit_bytes, std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp) {
  size_t size = StreamsSize();
  if (size > size_limit_bytes) {
    VLOG(1) << absl::Substitute("HTTP2 streams cleared due to size limit ($0 > $1).", size,
                                size_limit_bytes);
    streams_.clear();
  }

  EraseExpiredStreams(expiry_timestamp, &streams_);
}

protocols::http2::HalfStream* HTTP2StreamsContainer::HalfStreamPtr(uint32_t stream_id,
                                                                   bool write_event) {
  protocols::http2::Stream& stream = streams_[stream_id];

  if (stream.consumed) {
    // Don't expect this to happen, but log it just in case.
    // If http2/stitcher.cc uses std::move on consumption, this would indicate an issue.
    VLOG(1) << "Trying to access a consumed stream.";
  }

  protocols::http2::HalfStream* half_stream_ptr = write_event ? &stream.send : &stream.recv;
  return half_stream_ptr;
}

std::string HTTP2StreamsContainer::DebugString(std::string_view prefix) const {
  std::string info;
  info += absl::Substitute("$0streams=$1\n", prefix, streams_.size());
  return info;
}

}  // namespace stirling
}  // namespace px
