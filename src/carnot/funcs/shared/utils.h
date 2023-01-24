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

#include <sys/sysinfo.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
// Stringifies a vector, including 0 and 1 element inputs.
inline types::StringValue VectorToStringArray(const std::vector<std::string>& vec) {
  rapidjson::StringBuffer s;
  rapidjson::Writer<rapidjson::StringBuffer> writer(s);

  writer.StartArray();
  for (const auto& str : vec) {
    writer.String(str.c_str());
  }
  writer.EndArray();
  return s.GetString();
}

// Stringifies a vector, but returns the input only as a vector for size>=2.
inline types::StringValue StringifyVector(const std::vector<std::string>& vec) {
  if (vec.size() == 1) {
    return std::string(vec[0]);
  } else if (vec.size() > 1) {
    return VectorToStringArray(vec);
  }
  return "";
}

}  // namespace funcs
}  // namespace carnot
}  // namespace px
