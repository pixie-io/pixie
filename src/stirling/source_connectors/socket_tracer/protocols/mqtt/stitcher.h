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
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/types.h"


namespace px {
namespace stirling {
namespace protocols {

template <>
inline RecordsWithErrorCount<mqtt::Record> StitchFrames(std::deque<mqtt::Message>* req_messages,
                                                        std::deque<mqtt::Message>* resp_messages,
                                                        NoState* /*state*/) {
  return StitchMessagesWithTimestampOrder<mqtt::Record>(req_messages, resp_messages);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
