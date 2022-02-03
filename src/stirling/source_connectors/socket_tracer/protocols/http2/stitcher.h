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
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/http2_streams_container.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http2 {

// Outputs HTTP2 messages with the same stream ID into req & resp records.
// Incomplete records are not exported, they might be erased when the parent ConnTracker is
// destroyed.
void ProcessHTTP2Streams(HTTP2StreamsContainer* http2_streams,
                         RecordsWithErrorCount<http2::Record>* result);

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace px
