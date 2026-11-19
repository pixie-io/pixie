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

#include <optional>
#include <string>
#include <string_view>

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
  size_t payload_size = 0;

  std::string ToString() const {
    return absl::Substitute(
        "Message: {type: $0, opcode: $1, compressed: $2, fragments: $3, len: $4}",
        magic_enum::enum_name(type), magic_enum::enum_name(opcode), compressed, fragments,
        payload_size);
  }
};

enum class RecordKind : uint8_t {
  kFrame,
  kExec,
  kInput,
  kExit,
};

inline std::string_view RecordKindName(RecordKind kind) {
  switch (kind) {
    case RecordKind::kFrame:
      return "frame";
    case RecordKind::kExec:
      return "exec";
    case RecordKind::kInput:
      return "input";
    case RecordKind::kExit:
      return "exit";
  }
  return "unknown";
}

struct Record {
  Message msg;
  RecordKind kind = RecordKind::kFrame;
  std::string subprotocol;
  std::string output;
  size_t output_size = 0;

  std::string ToString() const {
    return absl::Substitute("[kind=$0 subprotocol=$1 msg=$2 output_size=$3]", RecordKindName(kind),
                            subprotocol, msg.ToString(), output_size);
  }
};

struct TarEntryInfo {
  std::string name;
  uint64_t mode = 0;
  uint64_t size = 0;
  std::string head;
};

struct TarStream {
  bool checked = false;
  bool is_tar = false;
  bool done = false;
  bool truncated = false;
  bool long_name = false;
  std::string held;
  uint64_t skip = 0;
  uint64_t capture_left = 0;
  std::string next_name;
  std::optional<TarEntryInfo> entry;
  std::string manifest;
};

struct PendingInput {
  uint64_t timestamp_ns = 0;
  std::string line;
  bool echoed = false;
};

struct ExecState {
  bool started = false;
  bool exited = false;
  uint64_t last_timestamp_ns = 0;
  bool tty = false;
  bool last_cr = false;
  bool echoed = false;
  std::string line;
  uint64_t line_timestamp_ns = 0;
  std::optional<PendingInput> pending;
  std::string output;
  size_t output_size = 0;
  TarStream stdout_tar;
  uint64_t stdin_timestamp_ns = 0;
  std::string stdin_data;
  size_t stdin_size = 0;
  TarStream stdin_tar;
};

struct State {
  std::string path;
  std::string subprotocol;
  bool conn_closed = false;
  uint32_t stitches_since_close = 0;
  ExecState exec;
};

struct DirectionState {
  uint64_t skip = 0;
};

struct StateWrapper {
  State global;
  DirectionState send;
  DirectionState recv;
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = StateWrapper;
  using key_type = stream_id_t;

  static void ConvertTimestamps(Record* record, ConvertTimestampsFuncType func) {
    record->msg.timestamp_ns = func(record->msg.timestamp_ns);
  }
};

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
