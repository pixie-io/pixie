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

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"

namespace px {
namespace stirling {
namespace protocols {
namespace websocket {

using stream_id_t = uint16_t;

enum class Opcode : uint8_t {
  kContinuation = 0x0,
  kText = 0x1,
  kBinary = 0x2,
  kClose = 0x8,
  kPing = 0x9,
  kPong = 0xa,
};

inline bool IsControl(Opcode opcode) { return static_cast<uint8_t>(opcode) & 0x8; }

struct Frame : public FrameBase {
  bool fin = false;
  uint8_t rsv = 0;
  Opcode opcode = Opcode::kContinuation;
  bool masked = false;
  uint64_t payload_length = 0;
  std::string payload;

  size_t ByteSize() const override { return sizeof(Frame) + payload.size(); }

  std::string ToString() const override {
    return absl::Substitute("Frame: {fin: $0, rsv: $1, opcode: $2, masked: $3, len: $4}", fin, rsv,
                            magic_enum::enum_name(opcode), masked, payload_length);
  }
};

struct Message {
  uint64_t timestamp_ns = 0;
  message_type_t type = message_type_t::kUnknown;
  Opcode opcode = Opcode::kContinuation;
  bool compressed = false;
  size_t fragments = 0;
  std::string payload;

  std::string ToString() const {
    return absl::Substitute("Message: {type: $0, opcode: $1, compressed: $2, fragments: $3, len: $4}",
                            magic_enum::enum_name(type), magic_enum::enum_name(opcode), compressed,
                            fragments, payload.size());
  }
};

struct Record {
  Message msg;

  std::string ToString() const { return absl::Substitute("[msg=$0]", msg.ToString()); }
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = NoState;
  using key_type = stream_id_t;

  static void ConvertTimestamps(Record* record, ConvertTimestampsFuncType func) {
    record->msg.timestamp_ns = func(record->msg.timestamp_ns);
  }
};

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
