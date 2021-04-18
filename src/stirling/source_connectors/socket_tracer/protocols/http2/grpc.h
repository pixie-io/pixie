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

namespace px {
namespace stirling {
namespace grpc {

constexpr size_t kGRPCMessageHeaderSizeInBytes = 5;

/**
 * Parses the input str as the provided protobuf message type.
 * Essentially a wrapper around PBWireToText that is easier to use.
 *
 * @param str The raw message as a string.
 * @param str_truncation_len The string length of any string/bytes fields beyond which truncation
 *        applies, if specified.
 * @return The parsed message.
 */
std::string ParsePB(std::string_view str, std::optional<int> str_truncation_len = std::nullopt);

}  // namespace grpc
}  // namespace stirling
}  // namespace px
