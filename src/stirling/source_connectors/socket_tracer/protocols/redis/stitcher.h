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
#include <limits>
#include <utility>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/timestamp_stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"

namespace px {
namespace stirling {
namespace protocols {

template <>
inline RecordsWithErrorCount<redis::Record> StitchFrames(std::deque<redis::Message>* req_messages,
                                                         std::deque<redis::Message>* resp_messages,
                                                         NoState* /* state */) {
  // NOTE: This cannot handle Redis pipelining if there is any missing message.
  // See https://redis.io/topics/pipelining for Redis pipelining.
  //
  // This is copied from StitchMessagesWithTimestampOrder() in timestamp_stitcher.h.
  // With additionally always pushing published messages into records with a synthesized request.
  // See below.

  std::vector<redis::Record> records;

  redis::Record record;
  record.req.timestamp_ns = 0;
  record.resp.timestamp_ns = 0;

  redis::Message placeholder_message;
  placeholder_message.timestamp_ns = std::numeric_limits<int64_t>::max();

  auto req_iter = req_messages->begin();
  auto resp_iter = resp_messages->begin();
  while (req_iter != req_messages->end() || resp_iter != resp_messages->end()) {
    redis::Message& req = (req_iter == req_messages->end()) ? placeholder_message : *req_iter;
    redis::Message& resp = (resp_iter == resp_messages->end()) ? placeholder_message : *resp_iter;

    // This if block is the added code to StitchMessagesWithTimestampOrder().
    // For Redis pub/sub, published messages have no corresponding `requests`, therefore we
    // forcefully turn them into records without requests.
    if (resp_iter != resp_messages->end() && resp.is_published_message) {
      // Synthesize the request message.
      redis::Record unstitched_record = {};

      unstitched_record.req.timestamp_ns = resp.timestamp_ns;
      constexpr std::string_view kPubPushCmd = "PUSH PUB";
      unstitched_record.req.command = kPubPushCmd;

      unstitched_record.resp = std::move(resp);

      records.push_back(std::move(unstitched_record));
      ++resp_iter;
      continue;
    }

    // Handle REPLCONF ACK command sent from follower to leader.
    constexpr std::string_view kReplConfAck = "REPLCONF ACK";
    if (req_iter != req_messages->end() && req.command == kReplConfAck &&
        // Ensure the output order based on timestamps.
        (resp_iter == resp_messages->end() || req.timestamp_ns < resp.timestamp_ns)) {
      redis::Record unstitched_record = {};

      unstitched_record.req = std::move(req);
      unstitched_record.resp.timestamp_ns = req.timestamp_ns;

      records.push_back(std::move(unstitched_record));
      ++req_iter;
      continue;
    }

    // Handle commands sent from leader to follower, which are replayed at the follower.
    if (resp_iter != resp_messages->end() && !resp.command.empty()) {
      redis::Record unstitched_record = {};

      unstitched_record.req = std::move(resp);
      unstitched_record.resp.timestamp_ns = resp.timestamp_ns;
      unstitched_record.role_swapped = true;

      records.push_back(std::move(unstitched_record));
      ++resp_iter;
      continue;
    }

    if (req.timestamp_ns < resp.timestamp_ns) {
      record.req = std::move(req);
      ++req_iter;
    } else {
      if (record.req.timestamp_ns != 0) {
        record.resp = std::move(resp);

        records.push_back(std::move(record));

        record.req.timestamp_ns = 0;
        record.resp.timestamp_ns = 0;
      }
      ++resp_iter;
    }
  }

  req_messages->erase(req_messages->begin(), req_iter);
  resp_messages->erase(resp_messages->begin(), resp_iter);

  return {std::move(records), 0};
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
