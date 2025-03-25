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
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

/**
 * StitchFrames is the entry point of the Cassandra Stitcher. It loops through the resp_frames,
 * matches them with the corresponding req_frames, and optionally produces an entry to emit.
 *
 * @param req_frames: deque of all request frames.
 * @param resp_frames: deque of all response frames.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> StitchFrames(
    absl::flat_hash_map<stream_id_t, std::deque<Frame>>* req_frames,
    absl::flat_hash_map<stream_id_t, std::deque<Frame>>* resp_frames);

}  // namespace cass

template <>
inline RecordsWithErrorCount<cass::Record> StitchFrames(
    absl::flat_hash_map<cass::stream_id_t, std::deque<cass::Frame>>* req_messages,
    absl::flat_hash_map<cass::stream_id_t, std::deque<cass::Frame>>* res_messages,
    NoState* /* state */) {
  return cass::StitchFrames(req_messages, res_messages);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
