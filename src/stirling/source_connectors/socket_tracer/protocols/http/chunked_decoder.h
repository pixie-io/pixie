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

#include "src/stirling/utils/parse_state.h"

// Choose either the pico or custom implementation of the chunked HTTP body decoder.
DECLARE_bool(use_pico_chunked_decoder);

namespace px {
namespace stirling {
namespace protocols {
namespace http {

/**
 * Parse an HTTP chunked body.
 *
 * @param buf The input data buffer. If parsing succeeds, the corresponding bytes are consumed;
 *            otherwise the string_view bytes are not modified.
 * @param result Result where the decoded chunked message is placed upon success.
 * @return ParseState::kInvalid if message is malformed.
 *         ParseState::kNeedsMoreData if the message is incomplete.
 *         ParseState::kSuccess if the chunk length was extracted and chunk header is well-formed.
 */
ParseState ParseChunked(std::string_view* buf, std::string* result);

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
