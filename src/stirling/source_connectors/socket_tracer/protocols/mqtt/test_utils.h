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
#include <vector>

#include "src/common/base/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mqtt {
namespace testutils {

inline Message CreateFrame(message_type_t type, const MqttControlPacketType control_packet_type,
                           uint32_t packet_identifier, uint32_t qos) {
  Message f;

  f.type = type;
  f.control_packet_type = magic_enum::enum_integer(control_packet_type);
  f.header_fields["packet_identifier"] = packet_identifier;
  f.header_fields["qos"] = qos;

  return f;
}

}  // namespace testutils
}  // namespace mqtt
}  // namespace protocols
}  // namespace stirling
}  // namespace px
