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

void FindMoreToComeResponses(
    absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>* resps, int* error_count,
    mongodb::Frame* resp_frame, uint64_t* latest_resp_ts) {
  // In a more to come message, the response frame's responseTo will be the requestID of the prior
  // response frame.

  // Find and insert all of the more to come frame(s) section data to the head response frame.
  auto curr_resp = resp_frame;

  while (curr_resp->more_to_come) {
    // Find the next response's deque.
    auto next_resp_deque_it = resps->find(curr_resp->request_id);
    if (next_resp_deque_it == resps->end()) {
      VLOG(1) << absl::Substitute(
          "Did not find a response deque extending the prior more to come response. "
          "requestID: $0",
          curr_resp->request_id);
      (*error_count)++;
      return;
    }

    // Response deque containing the next more to come response frame.
    auto& next_resp_deque = next_resp_deque_it->second;

    // Find the next response frame from the deque with a timestamp just greater than the
    // current response frame's timestamp.
    auto next_resp_it = std::upper_bound(
        next_resp_deque.begin(), next_resp_deque.end(), *latest_resp_ts,
        [](const uint64_t ts, const mongodb::Frame& frame) { return ts < frame.timestamp_ns; });
    if (next_resp_it->timestamp_ns < *latest_resp_ts) {
      VLOG(1) << absl::Substitute(
          "Did not find a response extending the prior more to come response. RequestID: $0",
          curr_resp->request_id);
      (*error_count)++;
      return;
    }

    // Insert the next response's section data to the head of the more to come response.
    mongodb::Frame& next_resp = *next_resp_it;
    resp_frame->sections.insert(std::end(resp_frame->sections), std::begin(next_resp.sections),
                                std::end(next_resp.sections));
    next_resp.consumed = true;
    *latest_resp_ts = next_resp.timestamp_ns;
    curr_resp = &next_resp;
  }

  // TODO(kpattaswamy): In the case of "missing" more to come middle/tail frames, determine whether
  // they are truly missing or have not been parsed yet.
}

void FlattenSections(mongodb::Frame* frame) {
  // Flatten the vector of sections containing vector of documents into a single string.
  for (const auto& section : frame->sections) {
    for (const auto& doc : section.documents) {
      frame->frame_body.append(doc).append(" ");
    }
  }
  frame->sections.clear();
}

RecordsWithErrorCount<mongodb::Record> StitchFrames(
    absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>* reqs,
    absl::flat_hash_map<mongodb::stream_id_t, std::deque<mongodb::Frame>>* resps, State* state) {
  std::vector<mongodb::Record> records;
  int error_count = 0;

  for (auto& stream_id_pair : state->stream_order) {
    auto stream_id = stream_id_pair.first;

    // Find the stream ID's response deque.
    auto resp_it = resps->find(stream_id);
    if (resp_it == resps->end()) {
      VLOG(1) << absl::Substitute("Did not find a response deque with the stream ID: $0",
                                  stream_id);
      continue;
    }

    // Response deque for the stream ID.
    auto& resp_deque = resp_it->second;

    // Find the stream ID's request deque.
    auto req_it = reqs->find(stream_id);
    // The request deque should exist in the reqs map since the state contained the stream ID.
    CTX_DCHECK(req_it != reqs->end());

    // Request deque for the stream ID.
    auto& req_deque = req_it->second;

    // Track the latest response timestamp to compare against request frame's timestamp later.
    uint64_t latest_resp_ts = 0;

    // Stitch the first frame in the response deque with the corresponding request frame.
    for (auto& resp_frame : resp_deque) {
      if (resp_frame.consumed) {
        continue;
      }

      latest_resp_ts = resp_frame.timestamp_ns;

      // Find the corresponding request frame for the head response frame.
      auto req_frame_it = std::upper_bound(
          req_deque.begin(), req_deque.end(), latest_resp_ts,
          [](const uint64_t ts, const mongodb::Frame& frame) { return ts < frame.timestamp_ns; });

      if (req_frame_it != req_deque.begin()) {
        --req_frame_it;
      }

      if (req_frame_it->timestamp_ns > latest_resp_ts) {
        VLOG(1) << absl::Substitute(
            "Did not find a request frame that is earlier than the response. Response's "
            "responseTo: $0",
            resp_frame.response_to);
        resp_frame.consumed = true;
        error_count++;
        break;
      }

      mongodb::Frame& req_frame = *req_frame_it;

      FindMoreToComeResponses(resps, &error_count, &resp_frame, &latest_resp_ts);

      // Stitch the request/response and add it to the records.
      req_frame.consumed = true;
      resp_frame.consumed = true;
      FlattenSections(&req_frame);
      FlattenSections(&resp_frame);

      // Ignore stitching the request/response if either one is a handshaking frame.
      if (req_frame.is_handshake || resp_frame.is_handshake) {
        req_frame.consumed = true;
        resp_frame.consumed = true;
        break;
      }

      records.push_back({std::move(req_frame), std::move(resp_frame)});
      break;
    }

    auto erase_until_iter = req_deque.begin();
    while (erase_until_iter != req_deque.end() &&
           (erase_until_iter->consumed || erase_until_iter->timestamp_ns < latest_resp_ts)) {
      if (!erase_until_iter->consumed) {
        error_count++;
      }
      ++erase_until_iter;
    }

    req_deque.erase(req_deque.begin(), erase_until_iter);
    stream_id_pair.second = true;
  }

  // Clear the response deques.
  for (auto it = resps->begin(); it != resps->end(); it++) {
    auto& resp_deque = it->second;
    for (auto& resp : resp_deque) {
      if (!resp.consumed) {
        error_count++;
      }
    }
    resp_deque.clear();
  }

  // Clear the state.
  auto it = state->stream_order.begin();
  while (it != state->stream_order.end()) {
    if (it->second) {
      it = state->stream_order.erase(it);
    } else {
      it++;
    }
  }

  return {records, error_count};
}

}  // namespace mongodb
}  // namespace protocols
}  // namespace stirling
}  // namespace px
