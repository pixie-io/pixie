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

#include <map>
#include <string>
#include <utility>
#include <variant>

#include <absl/strings/str_replace.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mongodb {

RecordsWithErrorCount<mongodb::Record> StitchFrames(
    std::map<mongodb::stream_id, std::deque<mongodb::Frame>>* reqs,
    std::map<mongodb::stream_id, std::deque<mongodb::Frame>>* resps) {
  // The reqs map's key will be the requestID and value will be the deque containing the request frame.
  // The resps map's key will be the responseTo and value will be the deque containing the response frame. 
  std::vector<mongodb::Record> records;
  int error_count = 0;

  for (auto& [request_id, req_deque] : *reqs) {
    // Find the response frame that corresponds to the request frame.
    // There will only be one frame in each deque for each requestID/responseTo.
    auto& req = req_deque[0];
    auto itr = resps->find(request_id);
    if (itr == resps->end()) {
      VLOG(1) << absl::Substitute(
          "Did not find a response frame matching the request. Request's RequestID = $0",
          req.request_id);
      continue;
    }

    auto curr_resp = itr->second[0];
    if (req.timestamp_ns > curr_resp.timestamp_ns) {
      VLOG(1) << absl::Substitute(
          "The request occured after the response was recorded, Request's time stamp = $0 "
          "Response's time stamp = $1",
          req.timestamp_ns, curr_resp.timestamp_ns);
      error_count++;
    }

    // Keep track of the head of the response frame(s) for a potential more to come scenario.
    auto& head_resp_frame = itr->second[0];

    // Find the remaining frames that comprise of the full response message and insert their
    // sections to the head response frame's section vector.
    while (curr_resp.more_to_come) {
      auto itr = resps->find(curr_resp.request_id);
      if (itr == resps->end()) {
        VLOG(1) << "Did not find a response frame extending the prior more to come response";
        error_count++;
        break;
      }

      auto& next_resp = itr->second[0];
      if (curr_resp.timestamp_ns > next_resp.timestamp_ns) {
        VLOG(1) << absl::Substitute(
            "The response occured before the prior response was recorded. Prior response's time "
            "stamp = $0, current response's time stamp = $1",
            curr_resp.timestamp_ns, next_resp.timestamp_ns);
        error_count++;
        break;
      }
      
      // Insert the next response's sections to the head response frame.
      head_resp_frame.sections.insert(std::end(head_resp_frame.sections),
                                      std::begin(next_resp.sections), std::end(next_resp.sections));
      next_resp.consumed = true;
      curr_resp = next_resp;
      resps->erase(next_resp.response_to);
    }

    // Stitch the request frame with the response frame(s).
    req.consumed = true;
    head_resp_frame.consumed = true;
    records.push_back({std::move(req), std::move(head_resp_frame)});

    resps->erase(head_resp_frame.response_to);
  }

  for (const auto& [response_to, resp] : *resps) {
    VLOG(1) << absl::Substitute("Did not find a request matching the response. responseTo = $0",
                                response_to);
    error_count++;
  }

  // TODO (kpattaswamy) Cleanup stale requests.
  auto itr = reqs->begin();
  while (itr != reqs->end()) {
    auto& req = itr->second[0];
    if (req.consumed) {
      reqs->erase(itr++);
    } else {
      break;
    }
  }
  resps->clear();

  return {records, error_count};
}

}  // namespace mongodb
}  // namespace protocols
}  // namespace stirling
}  // namespace px
