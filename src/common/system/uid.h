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

#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include "src/common/base/statusor.h"

namespace px {

StatusOr<std::string> NameForUID(uid_t uid);

/**
 * Returns a map from UID to username by parsing the content of /etc/passwd file or an equivalent
 * file at different path.
 */
std::map<uid_t, std::string> ParsePasswd(std::string_view passwd_content);

}  // namespace px
