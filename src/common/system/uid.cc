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

#include "src/common/system/uid.h"

#include <pwd.h>
#include <unistd.h>

#include <system_error>

#include "src/common/base/error.h"
#include "src/common/base/utils.h"

namespace px {

StatusOr<std::string> NameForUID(uid_t uid) {
  struct passwd pwd = {};
  struct passwd* result = nullptr;
  std::string buf;
  int rc = ERANGE;

  constexpr int kInitialBufSize = 256;
  constexpr int kMaximalBufSize = 16 * 1024;
  // Iteratively double buffer size until a limit. ERANGE indicates that the provided buffer size is
  // too small.
  for (int buf_size = kInitialBufSize; buf_size < kMaximalBufSize && rc == ERANGE; buf_size *= 2) {
    buf.resize(buf_size);
    rc = getpwuid_r(uid, &pwd, buf.data(), buf.size(), &result);
  }
  if (rc != 0) {
    return error::System(std::error_code(rc, std::system_category()).message());
  }
  if (result == nullptr) {
    return error::NotFound("UID '$0' is not found", uid);
  }
  return std::string(pwd.pw_name);
}

std::map<uid_t, std::string> ParsePasswd(std::string_view passwd_content) {
  std::map<uid_t, std::string> res;
  std::vector<std::string_view> lines = GetLines(passwd_content);
  for (auto line : lines) {
    std::vector<std::string_view> fields = absl::StrSplit(line, ":");
    constexpr int kPasswdEntryFieldCount = 7;
    if (fields.size() < kPasswdEntryFieldCount) {
      continue;
    }
    constexpr int kUidFieldIdx = 2;
    uid_t uid = 0;
    if (!absl::SimpleAtoi(fields[kUidFieldIdx], &uid)) {
      continue;
    }
    res[uid] = fields[0];
  }
  return res;
}

}  // namespace px
