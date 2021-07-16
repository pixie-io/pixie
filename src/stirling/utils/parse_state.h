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
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

enum class ParseState {
  kUnknown,

  // The parse failed: data is invalid.
  // Input buffer consumed is not consumed and parsed output element is invalid.
  kInvalid,

  // The parse is partial: data appears to be an incomplete message.
  // Input buffer may be partially consumed and the parsed output element is not fully populated.
  kNeedsMoreData,

  // The parse succeeded, but the data is ignored.
  // Input buffer is consumed, but the parsed output element is invalid.
  kIgnored,

  // The parse succeeded, but indicated the end-of-stream.
  // Input buffer is consumed, and the parsed output element is valid.
  // however, caller should stop parsing any future data on this stream, even if more data exists.
  // Use cases include messages that indicate a change in protocol (see HTTP status 101).
  kEOS,

  // The parse succeeded.
  // Input buffer is consumed, and the parsed output element is valid.
  kSuccess,
};

inline ParseState TranslateStatus(const Status& status) {
  if (error::IsNotFound(status) || error::IsResourceUnavailable(status)) {
    return ParseState::kNeedsMoreData;
  }
  if (!status.ok()) {
    return ParseState::kInvalid;
  }
  return ParseState::kSuccess;
}

}  // namespace stirling
}  // namespace px
