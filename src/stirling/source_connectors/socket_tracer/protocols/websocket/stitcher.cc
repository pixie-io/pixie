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

#include <algorithm>
#include <utility>
#include <string_view>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/k8s_exec.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/parse.h"

namespace px {
namespace stirling {
namespace protocols {

namespace websocket {
namespace {

constexpr uint8_t kRsv1 = 0x4;
constexpr std::string_view kCompressedPlaceholder = "<removed: compressed>";

Message StartMessage(message_type_t type, Frame* frame) {
  Message msg;
  msg.timestamp_ns = frame->timestamp_ns;
  msg.type = type;
  msg.opcode = frame->opcode;
  msg.compressed = frame->rsv & kRsv1;
  msg.fragments = 1;
  msg.payload = std::move(frame->payload);
  msg.payload_size = frame->payload_length;
  return msg;
}

int StitchDirection(message_type_t type, size_t payload_limit, std::deque<Frame>* frames,
                    std::vector<Message>* messages) {
  int error_count = 0;
  std::deque<Frame> pending;

  for (Frame& frame : *frames) {
    if (IsControl(frame.opcode)) {
      messages->push_back(StartMessage(type, &frame));
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
      const size_t room = payload_limit - std::min(payload_limit, msg.payload.size());
      msg.payload.append(pending[i].payload, 0, room);
      msg.payload_size += pending[i].payload_length;
      ++msg.fragments;
    }
    messages->push_back(std::move(msg));
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
                                                      websocket::StateWrapper* state) {
  RecordsWithErrorCount<websocket::Record> result;
  std::vector<websocket::Message> messages;
  const size_t payload_limit = websocket::PayloadLimitBytes(state);
  result.error_count += websocket::StitchDirection(message_type_t::kRequest, payload_limit,
                                                   req_frames, &messages);
  result.error_count += websocket::StitchDirection(message_type_t::kResponse, payload_limit,
                                                   resp_frames, &messages);

  websocket::State empty_state;
  websocket::State* global = state == nullptr ? &empty_state : &state->global;
  if (websocket::IsK8sChannelProtocol(global->subprotocol)) {
    std::stable_sort(messages.begin(), messages.end(),
                     [](const websocket::Message& a, const websocket::Message& b) {
                       return a.timestamp_ns < b.timestamp_ns;
                     });
    websocket::DecodeK8sExec(global->path, std::move(messages), &global->exec, &result.records);
  } else {
    for (websocket::Message& msg : messages) {
      if (msg.compressed) {
        msg.payload = websocket::kCompressedPlaceholder;
      }
      websocket::Record record;
      record.msg = std::move(msg);
      result.records.push_back(std::move(record));
    }
  }
  for (websocket::Record& record : result.records) {
    record.subprotocol = global->subprotocol;
  }
  return result;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
