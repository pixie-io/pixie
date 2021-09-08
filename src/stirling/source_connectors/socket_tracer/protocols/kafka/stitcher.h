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

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

/**
 * StitchFrames is the entry point of the Kafka Stitcher.
 *
 * @param req_frames: deque of all request frames.
 * @param resp_frames: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> StitchFrames(std::deque<Packet>* req_packets,
                                           std::deque<Packet>* resp_packets, State* state);

}  // namespace kafka

template <>
inline RecordsWithErrorCount<kafka::Record> StitchFrames(std::deque<kafka::Packet>* req_packets,
                                                         std::deque<kafka::Packet>* resp_packets,
                                                         kafka::StateWrapper* state) {
  return kafka::StitchFrames(req_packets, resp_packets, &state->global);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
