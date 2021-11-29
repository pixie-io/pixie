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

#include "src/stirling/obj_tools/utils.h"
#include <absl/strings/match.h>

namespace px {
namespace stirling {
namespace obj_tools {

bool MatchesSymbol(std::string_view symbol_name, const SymbolSearchPattern& search_pattern) {
  switch (search_pattern.match_type) {
    case SymbolMatchType::kExact:
      return symbol_name == search_pattern.name;
    case SymbolMatchType::kPrefix:
      return absl::StartsWith(symbol_name, search_pattern.name);
    case SymbolMatchType::kSuffix:
      return absl::EndsWith(symbol_name, search_pattern.name);
    case SymbolMatchType::kSubstr:
      return symbol_name.find(search_pattern.name) != std::string_view::npos;
  }
  // Extra return to pass GCC build.
  return false;
}

bool MatchesSymbolAny(std::string_view symbol_name,
                      const std::vector<SymbolSearchPattern>& search_patterns) {
  for (const auto& search_pattern : search_patterns) {
    if (MatchesSymbol(symbol_name, search_pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
