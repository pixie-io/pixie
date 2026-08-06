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

#include <deque>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

DECLARE_uint32(http_body_limit_bytes);

namespace px {
namespace stirling {
namespace protocols {

/**
 * Parses a single HTTP message from the input string.
 * A note on the lazy_parsing_enabled flag:
 * It signals to the http protocol parser to parse lazily.
 * Currently only used when we know that a contiguous section of the data stream buffer
 * (the head passed to the parser) ends with a gap due to an incomplete event from bpf.
 * Note that the http parser consumes bytes from the input buffer when parsing a partial frame
 * even if it is not pushed in the event parser. To preserve the behavior of kNeedsMoreData
 * for non-incomplete heads where we expect more data to arrive, we implement this flag in
 * conjunction with tracking whether a head contains a gap.
 */
template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, http::Message* frame,
                      http::StateWrapper* state, bool lazy_parsing_enabled);

template <>
size_t FindFrameBoundary<http::Message>(message_type_t type, std::string_view buf, size_t start_pos,
                                        http::StateWrapper* state);

}  // namespace protocols
}  // namespace stirling
}  // namespace px
