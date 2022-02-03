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

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/stitcher.h"

#include <utility>

namespace px {
namespace stirling {
namespace protocols {
namespace http2 {

void ProcessHTTP2Streams(HTTP2StreamsContainer* http2_streams_container,
                         RecordsWithErrorCount<http2::Record>* result) {
  auto& streams = *http2_streams_container->mutable_streams();
  auto iter = streams.begin();
  while (iter != streams.end()) {
    auto& stream = iter->second;
    if (
        // This is true when the stream_end headers event were received from bpf. In practice,
        // this event can be received earlier than other headers events, even if they were
        // submitted to the perf buffer later.
        stream.StreamEnded() &&
        // This avoids the case where stream_end headers event were received before other
        // headers events. Also gives time for late-arriving response headers to be received.
        stream.HasHeaders()) {
      result->records.emplace_back(std::move(stream));
      streams.erase(iter++);
    } else {
      ++iter;
    }
  }
}

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace px
