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

#include <map>
#include <string>

#include "src/common/base/utils.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase.

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

using ::px::utils::ToJSONString;

// The protocol specification : https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.pdf
// This supports MQTTv5

struct Message : public FrameBase {
  message_type_t type = message_type_t::kUnknown;

  uint8_t control_packet_type = 0xff;

  bool dup;
  bool retain;

  std::map<std::string, uint32_t> header_fields;
  std::map<std::string, std::string> properties, payload;

  size_t ByteSize() const override { return sizeof(Message) + payload.size(); }

  std::string ToString() const override {
    return absl::Substitute(
        "Message: {type: $0, control_packet_type: $1, dup: $2, retain: $3, header_fields: $4, "
        "payload: $5, properties: $6}",
        magic_enum::enum_name(type), control_packet_type, dup, retain, ToJSONString(header_fields),
        ToJSONString(payload), ToJSONString(properties));
  }
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

/**
 *  Record is the primary output of the MQTT stitcher.
 */
struct Record {
  Message req;
  Message resp;

  std::string ToString() const {
    return absl::Substitute("[req=$0 resp=$1]", req.ToString(), resp.ToString());
  }
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Message;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace mqtt
}  // namespace protocols
}  // namespace stirling
}  // namespace px
