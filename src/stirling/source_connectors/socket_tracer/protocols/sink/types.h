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

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase

namespace px {
namespace stirling {
namespace protocols {
namespace sink {

// Represents a sink message.
struct Message : public FrameBase {
  size_t ByteSize() const override { return 0; }

  std::string ToString() const override {
    return absl::Substitute("base=[$0] payload=None.", FrameBase::ToString());
  }
};

// Represents a pair of request and response messages.
struct Record {
  Message req;
  Message resp;
  bool role_swapped = false;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1] role_swapped=$2", req.ToString(), resp.ToString(),
                            role_swapped);
  }
};

// Required by event parser interface.
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Message;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace sink
}  // namespace protocols
}  // namespace stirling
}  // namespace px
