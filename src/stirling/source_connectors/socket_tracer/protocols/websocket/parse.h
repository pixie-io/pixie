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

#include <string_view>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/types.h"

DECLARE_uint32(websocket_payload_limit_bytes);

namespace px {
namespace stirling {
namespace protocols {

namespace websocket {

size_t PayloadLimitBytes(const StateWrapper* state);

}  // namespace websocket

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, websocket::Frame* frame,
                      websocket::StateWrapper* state);

template <>
size_t FindFrameBoundary<websocket::Frame>(message_type_t type, std::string_view buf,
                                           size_t start_pos, websocket::StateWrapper* state);

}  // namespace protocols
}  // namespace stirling
}  // namespace px
