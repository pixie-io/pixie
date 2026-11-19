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

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/stitcher.h"

#include <utility>
#include <vector>

namespace px {
namespace stirling {
namespace protocols {

namespace websocket {
namespace {

constexpr uint8_t kRsv1 = 0x4;

Message StartMessage(message_type_t type, Frame* frame) {
  Message msg;
  msg.timestamp_ns = frame->timestamp_ns;
  msg.type = type;
  msg.opcode = frame->opcode;
  msg.compressed = frame->rsv & kRsv1;
  msg.fragments = 1;
  msg.payload = std::move(frame->payload);
  return msg;
}

int StitchDirection(message_type_t type, std::deque<Frame>* frames, std::vector<Record>* records) {
  int error_count = 0;
  std::deque<Frame> pending;

  for (Frame& frame : *frames) {
    if (IsControl(frame.opcode)) {
      records->push_back({StartMessage(type, &frame)});
      continue;
    }
    if (frame.opcode == Opcode::kContinuation) {
      if (pending.empty()) {
        ++error_count;
        continue;
      }
      pending.push_back(std::move(frame));
    } else {
      if (!pending.empty()) {
        ++error_count;
        pending.clear();
      }
      pending.push_back(std::move(frame));
    }
    if (!pending.back().fin) {
      continue;
    }
    Message msg = StartMessage(type, &pending.front());
    for (size_t i = 1; i < pending.size(); ++i) {
      msg.payload.append(pending[i].payload);
      ++msg.fragments;
    }
    records->push_back({std::move(msg)});
    pending.clear();
  }

  *frames = std::move(pending);
  return error_count;
}

}  // namespace
}  // namespace websocket

template <>
RecordsWithErrorCount<websocket::Record> StitchFrames(std::deque<websocket::Frame>* req_frames,
                                                      std::deque<websocket::Frame>* resp_frames,
                                                      NoState*) {
  RecordsWithErrorCount<websocket::Record> result;
  result.error_count +=
      websocket::StitchDirection(message_type_t::kRequest, req_frames, &result.records);
  result.error_count +=
      websocket::StitchDirection(message_type_t::kResponse, resp_frames, &result.records);
  return result;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
