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

#include <string>
#include <string_view>

#include <absl/container/flat_hash_map.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase

namespace px {
namespace stirling {
namespace protocols {
namespace nats {

// See https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#protocol-messages
// for the message format.
struct Message : public FrameBase {
  std::string command;

  // Formatted in JSON. Also includes payload, keyed by "payload".
  std::string options;

  size_t ByteSize() const override { return command.size() + options.size(); }

  std::string ToString() const override {
    return absl::Substitute("base=[$0] command=[$1] options=[$2]", FrameBase::ToString(), command,
                            options);
  }
};

// Represents a pair of request and response messages.
struct Record {
  Message req;

  // Error responses are always sent by server when encounter any error during processing the
  // request. OK responses are only sent by server in the verbose mode.
  // See https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#okerr.
  Message resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

using stream_id_t = uint16_t;
// Required by event parser interface.
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Message;
  using record_type = Record;
  using state_type = NoState;
  using key_type = stream_id_t;
};

constexpr std::string_view kInfo = "INFO";
constexpr std::string_view kConnect = "CONNECT";
constexpr std::string_view kPub = "PUB";
constexpr std::string_view kSub = "SUB";
constexpr std::string_view kUnsub = "UNSUB";
constexpr std::string_view kMsg = "MSG";
constexpr std::string_view kPing = "PING";
constexpr std::string_view kPong = "PONG";
constexpr std::string_view kOK = "+OK";
constexpr std::string_view kERR = "-ERR";

}  // namespace nats
}  // namespace protocols
}  // namespace stirling
}  // namespace px
