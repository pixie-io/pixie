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

#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/stitcher.h"

#include <absl/container/flat_hash_map.h>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/decode.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"

namespace px {
namespace stirling {
namespace protocols {
namespace amqp {
// AMQP protocols are in order within a channel.
// We can map each record to the next matching response within a channel.
// Some requests don't require a response(async). The async Frames will be sent back as records.
RecordsWithErrorCount<Record> StitchFrames(std::deque<Frame>* req_frames,
                                           std::deque<Frame>* resp_frames) {
  std::vector<Record> entries;
  int error_count = 0;
  // Maps channel_id to list of list of requests
  absl::flat_hash_map<int32_t, std::deque<Frame*>> req_channel_map;
  for (auto& req_frame : *req_frames) {
    if (req_frame.consumed) {
      continue;
    }

    if (req_frame.synchronous) {
      req_channel_map[req_frame.channel].push_back(&req_frame);
    } else {
      // If async, process requests and add it to entries
      req_frame.consumed = true;
      entries.push_back(Record{.req = std::move(req_frame), .resp = Frame{}});
    }
  }

  for (auto& resp_frame : *resp_frames) {
    if (resp_frame.consumed) {
      continue;
    }

    if (!resp_frame.synchronous) {
      resp_frame.consumed = true;
      entries.push_back(Record{.req = Frame{}, .resp = std::move(resp_frame)});
    } else {
      auto it = req_channel_map.find(resp_frame.channel);
      // In AMQP, sometimes server sends message to client first, and then client responses.
      // For example, server can send ConnectionStart or ConnectionTune, and client would respond
      // ConnectionStart-Ok or ConnecionTune-Ok. In these cases, having responses precede requests
      // is Ok.
      if (it == req_channel_map.end()) {
        continue;
      }
      if (req_channel_map[resp_frame.channel].size() == 0) {
        continue;
      }
      Frame* corresponding_req_frame = req_channel_map[resp_frame.channel].front();
      req_channel_map[resp_frame.channel].pop_front();

      corresponding_req_frame->consumed = true;
      resp_frame.consumed = true;
      entries.push_back(
          Record{.req = std::move(*corresponding_req_frame), .resp = std::move(resp_frame)});
    }
  }

  auto it = req_frames->begin();
  while (it != req_frames->end()) {
    if (!(*it).consumed) {
      break;
    }
    it++;
  }
  req_frames->erase(req_frames->begin(), it);

  // In AMQP, the responses might come before the requests. We need to clear only consumed
  // responses.
  auto resp_it = resp_frames->begin();
  while (resp_it != resp_frames->end()) {
    if (!(*resp_it).consumed) {
      break;
    }
    resp_it++;
  }
  resp_frames->erase(resp_frames->begin(), resp_it);

  return {entries, error_count};
}

}  // namespace amqp
}  // namespace protocols
}  // namespace stirling
}  // namespace px
