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

#include <string>

#include <absl/strings/numbers.h>
#include <absl/strings/substitute.h>
#include <absl/time/time.h>
#include "src/shared/version/version.h"

extern const char* kBuildSCMStatus;
extern const char* kBuildSCMRevision;
extern const int64_t kBuildTimeStamp;
extern const char* kBuildSemver;
extern const char* kBuildNumber;
extern const char* kBuiltBy;

namespace px {

std::string VersionInfo::Revision() { return kBuildSCMRevision; }

std::string VersionInfo::RevisionStatus() { return kBuildSCMStatus; }

std::string VersionInfo::Builder() { return kBuiltBy; }

int VersionInfo::BuildNumber() {
  int build_number = 0;
  bool ok = absl::SimpleAtoi(kBuildNumber, &build_number);
  return ok ? build_number : 0;
}

std::string VersionInfo::VersionString() {
#ifdef NDEBUG
  const char* build_type = "RELEASE";
#else
  const char* build_type = "DEBUG";
#endif
  std::string short_rev = Revision().substr(0, 7);
  auto t = absl::FromUnixSeconds(kBuildTimeStamp);
  auto build_time = absl::FormatTime("%Y%m%d%H%M", t, absl::LocalTimeZone());
  return absl::Substitute("v$0+$1.$2.$3.$4.$5.$6", kBuildSemver, RevisionStatus(), short_rev,
                          build_time, BuildNumber(), build_type, Builder());
}

}  // namespace px
