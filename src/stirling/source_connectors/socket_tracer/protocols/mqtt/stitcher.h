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
#include <string>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

/**
 * StitchFrames is the entry point of the MQTT Stitcher. It loops through the req_frames,
 * matches them with the corresponding resp_frames, and optionally produces an entry to emit.
 *
 * @param req_frames: deque of all request frames.
 * @param resp_frames: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> StitchFrames(
    absl::flat_hash_map<packet_id_t, std::deque<Message>>* req_frames,
    absl::flat_hash_map<packet_id_t, std::deque<Message>>* resp_frames);

}  // namespace mqtt

template <>
inline RecordsWithErrorCount<mqtt::Record> StitchFrames(
    absl::flat_hash_map<mqtt::packet_id_t, std::deque<mqtt::Message>>* req_messages,
    absl::flat_hash_map<mqtt::packet_id_t, std::deque<mqtt::Message>>* res_messages,
    NoState* /* state */) {
  return mqtt::StitchFrames(req_messages, res_messages);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
