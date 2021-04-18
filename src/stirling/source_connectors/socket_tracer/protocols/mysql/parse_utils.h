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

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

/**
 * Converts a length encoded int from string to int.
 * https://dev.mysql.com/doc/internals/en/integer.html#packet-Protocol::LengthEncodedInteger
 *
 * @param s the source bytes from which to extract the length-encoded int
 * @param offset the position at which to parse the length-encoded int.
 * The offset will be updated to point to the end position on a successful parse.
 * On an unsuccessful parse, the offset will be in an undefined state.
 *
 * @return int value if parsed, or error if there are not enough bytes to parse the int.
 */
StatusOr<int64_t> ProcessLengthEncodedInt(std::string_view s, size_t* offset);

/**
 * These dissectors are helper functions that parse out a parameter from a packet's raw contents.
 * The offset identifies where to begin the parsing, and the offset is updated to reflect where
 * the parsing ends.
 */
// TODO(oazizi): Convert Dissectors to use std::string_view*, and use remove_prefix().
// Then param_offset is no longer needed.

Status DissectStringParam(std::string_view msg, size_t* param_offset, std::string* packet);

template <size_t length>
Status DissectIntParam(std::string_view msg, size_t* param_offset, std::string* packet);

template <typename TFloatType>
Status DissectFloatParam(std::string_view msg, size_t* offset, std::string* packet);

Status DissectDateTimeParam(std::string_view msg, size_t* offset, std::string* packet);

// TODO(chengruizhe): infer length from int_type. Will require changes to DissectIntParam as well.
template <size_t length, typename int_type>
Status DissectInt(std::string_view msg, size_t* offset, int_type* result) {
  if (msg.size() < *offset + length) {
    return error::Internal("Not enough bytes to dissect int param.");
  }
  *result = utils::LEndianBytesToInt<int_type, length>(msg.substr(*offset));
  *offset += length;
  return Status::OK();
}
}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
