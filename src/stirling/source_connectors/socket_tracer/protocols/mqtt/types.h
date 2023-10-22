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

#include "src/common/base/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase.

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {

struct Message: public FrameBase {
    message_type_t type = message_type_t::kUnknown;

    std::string control_packet_type = "UNKNOWN";

    //TODO: Find the best way to store the payload
    std::map<std::string, uint32_t> header_fields;
    std::map<std::string, std::string> properties, payload;

    template<typename KeyType, typename ValueType>
    static std::string MapToString(const std::map<KeyType, ValueType>& inputMap) {
        std::string result = "{";
        for (const auto& entry : inputMap) {
            result += entry.first + ": ";
            if constexpr (std::is_same_v<ValueType, uint32_t>) {
                result += std::to_string(entry.second);
            } else if constexpr (std::is_same_v<ValueType, std::string>) {
                result += entry.second;
            }
            result += ", ";
        }
        if (!inputMap.empty()) {
            result = result.substr(0, result.size() - 2); // Remove the trailing ", "
        }
        result += "}";
        return result;
    }

    size_t ByteSize() const override {
        return sizeof(Message) + payload.size();
    }

    std::string ToString() const override {
        std::string header_fields_str = "{";
        for (const auto& entry : properties) {
            header_fields_str += entry.first + ": " + std::string(entry.second) + ", ";
        }
        header_fields_str += "}";

        std::string properties_str = "{";
        for (const auto& entry : properties) {
            properties_str += entry.first + ": " + entry.second + ", ";
        }
        properties_str += "}";

        std::string payload_str = "{";
        for (const auto& entry : properties) {
            payload_str += entry.first + ": " + entry.second + ", ";
        }
        payload_str += "}";

        return absl::Substitute(
          "Message: {type: $0, control_packet_type: $1, header_fields: $2, "
          "payload: $3, properties: $4}",
          magic_enum::enum_name(type), control_packet_type,
          header_fields_str, payload_str, properties_str);
    }
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

/**
 *  Record is the primary output of the http stitcher.
 */
struct Record{
    Message req;
    Message resp;


  std::string ToString() const {
    return absl::Substitute("[req=$0 resp=$1]", req.ToString(), resp.ToString());
  }
};

struct State {
  bool conn_closed = false;
};

struct StateWrapper {
  State global;
  std::monostate send;
  std::monostate recv;
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Message;
  using record_type = Record;
  using state_type = NoState;
};

} // namespace mqtt
} // namespace protocols
} // namespace stirling
} // namespace px