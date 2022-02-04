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
#include <string>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/mixins.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"

namespace px {
namespace stirling {

/**
 * HTTP2StreamsContainer is an object that holds the captured HTTP2 stream data from BPF.
 * This is managed differently from other protocols because it comes as UProbe data
 * and is already structured. This in contrast to other protocols which are captured via
 * KProbes and need to be parsed.
 */
class HTTP2StreamsContainer : NotCopyMoveable {
 public:
  const absl::flat_hash_map<uint32_t, protocols::http2::Stream>& streams() const {
    return streams_;
  }
  absl::flat_hash_map<uint32_t, protocols::http2::Stream>* mutable_streams() { return &streams_; }

  /**
   * Get the HTTP2 stream for the given stream ID and the direction of traffic.
   * @param write_event==true for send HalfStream, write_event==false for recv HalfStream.
   */
  protocols::http2::HalfStream* HalfStreamPtr(uint32_t stream_id, bool write_event);

  /**
   * Returns the approximate memory consumption of the HTTP2StreamsContainer.
   */
  size_t StreamsSize() const;

  /**
   * Cleans up the HTTP2 events from BPF uprobes that are too old,
   * either because they are too far back in time, or too far back in bytes.
   */
  void Cleanup(size_t size_limit_bytes,
               std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp);

  /**
   * Erase n stream IDs from the head of the streams container.
   */
  void EraseHead(size_t n);

  std::string DebugString(std::string_view prefix) const;

 private:
  // Map of all HTTP2 streams. Key is stream ID.
  absl::flat_hash_map<uint32_t, protocols::http2::Stream> streams_;
};

}  // namespace stirling
}  // namespace px
