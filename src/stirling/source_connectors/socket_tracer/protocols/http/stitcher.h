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
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/timestamp_stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

DECLARE_string(http_response_header_filters);

namespace px {
namespace stirling {
namespace protocols {
namespace http {

/**
 * ProcessMessages is the entry point of the HTTP Stitcher. It loops through the resp_packets,
 * matches them with the corresponding req_packets, and optionally produces an entry to emit.
 *
 * @param req_messages: deque of all request messages.
 * @param resp_messages: deque of all response messages.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> ProcessMessages(std::deque<Message>* req_messages,
                                              std::deque<Message>* resp_messages);

void PreProcessMessage(Message* message);

}  // namespace http

template <>
inline RecordsWithErrorCount<http::Record> StitchFrames(std::deque<http::Message>* req_messages,
                                                        std::deque<http::Message>* resp_messages,
                                                        http::StateWrapper* /* state */) {
  // NOTE: This cannot handle HTTP pipelining if there is any missing message.
  return StitchMessagesWithTimestampOrder<http::Record>(req_messages, resp_messages);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
