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

#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/common/base/status.h"
#include "src/common/base/statuspb/status.pb.h"

namespace px {
namespace error {

// Declare convenience functions:
// error::InvalidArgument(...)
// error::IsInvalidArgument(stat)
#define DECLARE_ERROR(FUNC, CONST)                                           \
  template <typename... Args>                                                \
  Status FUNC(std::string_view format, Args... args) {                       \
    return Status(::px::statuspb::CONST, absl::Substitute(format, args...)); \
  }                                                                          \
  inline bool Is##FUNC(const Status& status) { return status.code() == ::px::statuspb::CONST; }

DECLARE_ERROR(Cancelled, CANCELLED)
DECLARE_ERROR(Unknown, UNKNOWN)
DECLARE_ERROR(InvalidArgument, INVALID_ARGUMENT)
DECLARE_ERROR(DeadlineExceeded, DEADLINE_EXCEEDED)
DECLARE_ERROR(NotFound, NOT_FOUND)
DECLARE_ERROR(AlreadyExists, ALREADY_EXISTS)
DECLARE_ERROR(PermissionDenied, PERMISSION_DENIED)
DECLARE_ERROR(Unauthenticated, UNAUTHENTICATED)
DECLARE_ERROR(Internal, INTERNAL)
DECLARE_ERROR(Unimplemented, UNIMPLEMENTED)
DECLARE_ERROR(ResourceUnavailable, RESOURCE_UNAVAILABLE)
DECLARE_ERROR(System, SYSTEM)
DECLARE_ERROR(FailedPrecondition, FAILED_PRECONDITION)

#undef DECLARE_ERROR

inline std::string CodeToString(px::statuspb::Code code) {
  std::string_view code_str_view = magic_enum::enum_name(code);
  if (code_str_view.empty()) {
    return "Unknown error_code";
  }

  std::string code_str(code_str_view);
  // Example transformation: INVALID_ARGUMENT -> Invalid Argument
  int last = ' ';
  std::for_each(code_str.begin(), code_str.end(), [&last](char& c) {
    if (c == '_') {
      c = ' ';
    } else {
      c = (last == ' ') ? std::toupper(c) : std::tolower(c);
    }
    last = c;
  });

  return code_str;
}

}  // namespace error
}  // namespace px
