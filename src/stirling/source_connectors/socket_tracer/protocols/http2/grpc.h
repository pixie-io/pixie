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

#include <google/protobuf/message.h>

#include <optional>
#include <string>
#include <string_view>

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"

DECLARE_bool(socket_tracer_enable_http2_gzip);

namespace px {
namespace stirling {
namespace grpc {

/**
 * Parses protobuf body of a HTTP2 message.
 * Exported for testing.
 */
std::string ParsePB(std::string_view str, bool is_gzipped = false,
                    std::optional<int> str_field_truncation_len = std::nullopt);

/**
 * Parses the request & response body of the input HTTP2 Stream object.
 * Applies decompression & protobuf parsing if needed.
 *
 * @param http2_stream The input HTTP2 record whose request & response bodies are to be parsed,
 *        the results are written to the request & response bodies as well.
 * @param truncation_suffix The string suffix appended to any truncated string/bytes fields.
 * @param str_field_truncation_len The string length of any string/bytes fields beyond which
 *        truncation applies, if specified.
 */
void ParseReqRespBody(px::stirling::protocols::http2::Stream* http2_stream,
                      std::string_view truncation_suffix = {},
                      std::optional<int> str_truncation_len = std::nullopt);

}  // namespace grpc
}  // namespace stirling
}  // namespace px
