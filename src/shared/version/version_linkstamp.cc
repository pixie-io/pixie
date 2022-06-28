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

#include <cstdint>

extern const char* kBuildSCMStatus;
extern const char* kBuildSCMRevision;
extern const int64_t kBuildTimeStamp;
extern const char* kBuildSemver;
extern const char* kBuildNumber;
extern const char* kBuiltBy;

const char* kBuildSCMStatus = BUILD_SCM_STATUS;
const char* kBuildSCMRevision = BUILD_SCM_REVISION;
const int64_t kBuildTimeStamp = BUILD_TIMESTAMP;  // UNIX TIMESTAMP seconds.
const char* kBuildSemver = BUILD_TAG;             // Semver string.
const char* kBuildNumber = BUILD_NUMBER;          // Build number..
const char* kBuiltBy = BUILT_BY;
