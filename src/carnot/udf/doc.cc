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

#include <functional>
#include <regex>
#include <string>

#include "src/carnot/udf/doc.h"

namespace px {
namespace carnot {
namespace udf {

std::string DedentBlock(const std::string& in) {
  static std::regex dedenter{"\n.*\\|\\s"};
  auto s = std::regex_replace(std::string(in), dedenter, "\n");
  if (!s.empty() && s[0] == '\n') {
    s.erase(0, 1);
  }
  // Strip any trailing spaces.
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       std::bind(std::not_equal_to<char>(), std::placeholders::_1, ' '))
              .base(),
          s.end());

  return s;
}

}  // namespace udf
}  // namespace carnot
}  // namespace px
