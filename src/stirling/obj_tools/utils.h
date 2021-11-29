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

#include <string_view>
#include <vector>

namespace px {
namespace stirling {
namespace obj_tools {

enum class SymbolMatchType {
  // Search for a symbol that is an exact match of the search string.
  kExact,

  // Search for a symbol that starts with the search string.
  kPrefix,

  // Search for a symbol that ends with the search string.
  kSuffix,

  // Search for a symbol that contains the search string.
  kSubstr
};

// Describes how to match a symbol name.
struct SymbolSearchPattern {
  SymbolMatchType match_type;
  std::string_view name;
};

// Returns true if the input symbol matches the symbol pattern according to the match type.
bool MatchesSymbol(std::string_view symbol_name, const SymbolSearchPattern& search_pattern);

// Returns true if the input symbol matches any of the search patterns.
bool MatchesSymbolAny(std::string_view symbol_name,
                      const std::vector<SymbolSearchPattern>& search_patterns);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
