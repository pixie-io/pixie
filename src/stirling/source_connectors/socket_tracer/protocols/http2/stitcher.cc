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

void ProcessHTTP2Streams(HTTP2StreamsContainer* http2_streams_container, bool conn_closed,
                         RecordsWithErrorCount<http2::Record>* result) {
  auto& streams = *http2_streams_container->mutable_streams();
  auto iter = streams.begin();
  while (iter != streams.end()) {
    auto& stream = iter->second;
    if (conn_closed || stream.StreamEnded()) {
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
