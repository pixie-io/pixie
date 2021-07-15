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
namespace nats {

// See https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#protocol-messages
// for the message format.
struct Message : public FrameBase {
  std::string payload;
  size_t ByteSize() const override { return payload.size(); }
  std::string ToString() const override { return FrameBase::ToString(); }
};

}  // namespace nats
}  // namespace protocols
}  // namespace stirling
}  // namespace px
