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
#include "src/stirling/source_connectors/socket_tracer/protocols/nats/types.h"

namespace px {
namespace stirling {
namespace protocols {

template <>
size_t FindFrameBoundary<nats::Message>(message_type_t /*type*/, std::string_view buf,
                                        size_t start_pos, NoState* /*state*/);

template <>
ParseState ParseFrame(message_type_t /*type*/, std::string_view* buf, nats::Message* msg,
                      NoState* /*state*/, bool /*lazy_parsing_enabled*/);

}  // namespace protocols
}  // namespace stirling
}  // namespace px
