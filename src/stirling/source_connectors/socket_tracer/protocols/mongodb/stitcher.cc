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

#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/stitcher.h"

#include <string>
#include <utility>
#include <variant>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_replace.h>
#include "src/common/base/base.h"

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mongodb {

RecordsWithErrorCount<mongodb::Record> StitchFrames(
    absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>* reqs,
    absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>* resps, State* state) {

  std::vector<mongodb::Record> records;
  int error_count = 0;

  for (const auto& [idx, stream_id] : Enumerate(state->transaction_stream_order)) {
    // Find the stream ID's response deque.
    auto resp_it = resps->find(stream_id);
    if (resp_it == resps->end()) {
      VLOG(1) << absl::Substitute(
          "Did not find a response deque with the stream ID: $0",
          stream_id);
      error_count++;
      continue;
    }

    // Response deque for the stream ID.
    auto& resp_deque = resp_it->second;

    // Find the stream ID's response deque.
    auto req_it = reqs->find(stream_id);
    if (req_it == reqs->end()) {
      VLOG(1) << absl::Substitute(
          "Did not find a request with the stream ID = $0",
          stream_id);
      error_count++;
      continue;
    }

    // Request deque for the stream ID.
    auto& req_deque = req_it->second;

    // Track the latest response timestamp to compare against request frame's timestamp later.
    uint64_t latest_resp_ts = 0;

    // Loop over each frame in the response deque and match the oldest response frame with the oldest request that occured just before the response.
    for (const auto& [idx, resp_frame] : Enumerate(resp_deque)) {
      if (resp_frame.consumed) {
        continue;
      }

      latest_resp_ts = resp_frame.timestamp_ns;

      auto curr_resp = resp_frame;

      // Find and insert all of the moreToCome frame(s)' section data to the head response frame.
      while (curr_resp.more_to_come) {
        // Find the next response's deque.
        auto next_resp_deque_it = resps->find(curr_resp.request_id);
        if (next_resp_deque_it == resps->end()) {
          VLOG(1) << absl::Substitute(
              "Did not find a response deque with extending the prior more to come response. responseTo: $0",
              curr_resp.request_id);
          error_count++;
          break;
        }

        // Response deque containg the next more to come response frame.
        auto& next_resp_deque = next_resp_deque_it->second;

        // Find the next response frame from the deque with a timestamp just greater than the current response frame's timestamp.
        auto next_resp_it =
        std::upper_bound(next_resp_deque.begin(), next_resp_deque.end(), latest_resp_ts,
                        [](const uint64_t ts, const mongodb::Frame& frame) {
                          return ts < frame.timestamp_ns;
                        });
        if (next_resp_it->timestamp_ns < latest_resp_ts) {
          VLOG(1) << absl::Substitute("Did not find a response extending the prior more to come response. RequestID: $0", curr_resp.request_id);
          error_count++;
          continue;
        }

        mongodb::Frame& next_resp = *next_resp_it;
        resp_frame.sections.insert(std::end(resp_frame.sections), std::begin(next_resp.sections),
                                  std::end(next_resp.sections));
        next_resp.consumed = true;
        latest_resp_ts = next_resp.timestamp_ns;
        curr_resp = next_resp;
        next_resp_deque.erase(next_resp_it);
      }

      // Find the corresponding request frame for the head response frame.
      auto req_frame_it =
          std::upper_bound(req_deque.begin(), req_deque.end(), latest_resp_ts,
                          [](const uint64_t ts, const mongodb::Frame& frame) {
                            return ts < frame.timestamp_ns;
                          }) -
          1;
      if (req_frame_it == req_deque.begin() &&
          req_frame_it->timestamp_ns > resp_frame.timestamp_ns) {
        VLOG(1) << absl::Substitute("Did not find a request frame that is earlier than the response. Response's responseTo: $0", resp_frame.response_to);
        error_count++;
        continue;
      }

      mongodb::Frame& req_frame = *req_frame_it;
      req_frame.consumed = true;
      resp_frame.consumed = true;
      records.push_back({std::move(req_frame), std::move(resp_frame)});
      resp_deque.erase(resp_deque.begin() + idx);
    }

    size_t delete_idx = req_deque.size();
    bool found_unconsumed = false;
    for (const auto& [idx, frame] : Enumerate(req_deque)) {
      if (frame.consumed) {
        continue;
      }

      if (frame.discarded || frame.timestamp_ns < latest_resp_ts) {
        error_count++;
        frame.discarded = true;
      } else if (!found_unconsumed) {
        delete_idx = idx;
        found_unconsumed = true;
        break;
      }
    }
    req_deque.erase(req_deque.begin(), req_deque.begin() + delete_idx);
  }

  for (auto it = resps->begin(); it != resps->end(); it++) {
    auto& resp_deque = it->second;
    if (resp_deque.size() > 0) {
      error_count += resp_deque.size();
      resp_deque.clear();
    }
  }
  state->transaction_stream_order.clear();

  return {records, error_count};
}

}  // namespace mongodb
}  // namespace protocols
}  // namespace stirling
}  // namespace px