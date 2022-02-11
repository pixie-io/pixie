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

namespace px {
namespace stirling {
namespace protocols {

// Stitches request & response pairs based on the fact that the response's timestamp must be
// later than that of its corresponding request.
//
// NOTICE: This cannot handle pipelining.
//
// TRecordType must have 2 member variables that are of TMessageType. Something like:
// TRecordType {
//   TMessageType req;
//   TMessageType resp;
//   ...
// }
//
// TMessageType must have a `timestamp_ns` member variable. That's enforced by deriving from
// the FrameBase in event_parser.h.
template <typename TRecordType, typename TMessageType>
RecordsWithErrorCount<TRecordType> StitchMessagesWithTimestampOrder(
    std::deque<TMessageType>* req_messages, std::deque<TMessageType>* resp_messages) {
  std::vector<TRecordType> records;

  TRecordType record;
  record.req.timestamp_ns = 0;
  record.resp.timestamp_ns = 0;

  TMessageType placeholder_message;
  placeholder_message.timestamp_ns = std::numeric_limits<int64_t>::max();

  // Implementation resembles a merge-sort of the two deques.
  // Each iteration, we pop off either a request or response message.
  auto req_iter = req_messages->begin();
  auto resp_iter = resp_messages->begin();
  while (resp_iter != resp_messages->end()) {
    TMessageType& req = (req_iter == req_messages->end()) ? placeholder_message : *req_iter;
    TMessageType& resp = (resp_iter == resp_messages->end()) ? placeholder_message : *resp_iter;

    // Process the oldest item, either a request or response.
    if (req.timestamp_ns < resp.timestamp_ns) {
      // Requests always go into the record (though not pushed yet).
      // If the next oldest item is a request too, it will (correctly) clobber this one.
      record.req = std::move(req);
      ++req_iter;
    } else {
      // Two cases for a response:
      // 1) No older request was found: then we ignore the response.
      // 2) An older request was found: then it is considered a match. Push the record, and reset.
      if (record.req.timestamp_ns != 0) {
        record.resp = std::move(resp);
        records.push_back(std::move(record));

        // Reset record after pushing.
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
