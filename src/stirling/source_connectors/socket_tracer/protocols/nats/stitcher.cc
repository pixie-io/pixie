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

#include "src/stirling/source_connectors/socket_tracer/protocols/nats/stitcher.h"

namespace px {
namespace stirling {
namespace protocols {

namespace {

// Exports the message pointed by the input iterator to the records, and advance the iterator
// accordingly.
std::deque<nats::Message>::iterator ExportMessage(std::deque<nats::Message>::iterator iter,
                                                  std::vector<nats::Record>* records) {
  if (  // Ignore OK and ERR messages because they have no matching requests.
      iter->command == nats::kOK || iter->command == nats::kERR ||
      // TODO(yzhao): Skip for now. Implement this.
      iter->command == nats::kPing || iter->command == nats::kPong) {
    return ++iter;
  }
  records->push_back({std::move(*iter), {}});
  return ++iter;
}

}  // namespace

// PUB, SUB, MSG messages are exported as separate records, without matching between each other.
// +OK, -ERR messages are matched with earlier requests, and are ignored if no such requests exit.
// PING and PONG messages are left for followup.
template <>
RecordsWithErrorCount<nats::Record> StitchFrames(std::deque<nats::Message>* req_msgs,
                                                 std::deque<nats::Message>* resp_msgs,
                                                 NoState* /* state */) {
  std::vector<nats::Record> records;
  auto req_iter = req_msgs->begin();
  auto resp_iter = resp_msgs->begin();

  while (req_iter != req_msgs->end() && resp_iter != resp_msgs->end()) {
    if (resp_iter->command == nats::kOK || resp_iter->command == nats::kERR) {
      if (req_iter->timestamp_ns < resp_iter->timestamp_ns) {
        records.push_back({std::move(*req_iter), std::move(*resp_iter)});
        ++req_iter;
      }
      // If there is no request arriving earlier than the response, consume the response, assuming
      // that the request were lost.
      ++resp_iter;
      continue;
    }

    if (req_iter->timestamp_ns <= resp_iter->timestamp_ns) {
      req_iter = ExportMessage(req_iter, &records);
    } else {
      resp_iter = ExportMessage(resp_iter, &records);
    }
  }

  while (req_iter != req_msgs->end()) {
    req_iter = ExportMessage(req_iter, &records);
  }

  while (resp_iter != resp_msgs->end()) {
    resp_iter = ExportMessage(resp_iter, &records);
  }

  req_msgs->erase(req_msgs->begin(), req_iter);
  resp_msgs->erase(resp_msgs->begin(), resp_iter);

  return {std::move(records), 0};
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
