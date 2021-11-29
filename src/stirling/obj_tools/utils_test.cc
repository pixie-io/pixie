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

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace obj_tools {

TEST(MatchesSymbolTest, AsExpected) {
  EXPECT_TRUE(MatchesSymbol("test", {SymbolMatchType::kExact, "test"}));

  EXPECT_FALSE(MatchesSymbol("test_1", {SymbolMatchType::kExact, "test"}));
  EXPECT_FALSE(MatchesSymbol("test_1", {SymbolMatchType::kSuffix, "test"}));
  EXPECT_TRUE(MatchesSymbol("test_1", {SymbolMatchType::kSubstr, "test"}));
  EXPECT_TRUE(MatchesSymbol("test_1", {SymbolMatchType::kPrefix, "test"}));

  EXPECT_FALSE(MatchesSymbol("1_test", {SymbolMatchType::kExact, "test"}));
  EXPECT_TRUE(MatchesSymbol("1_test", {SymbolMatchType::kSuffix, "test"}));
  EXPECT_TRUE(MatchesSymbol("1_test", {SymbolMatchType::kSubstr, "test"}));
  EXPECT_FALSE(MatchesSymbol("1_test", {SymbolMatchType::kPrefix, "test"}));
}

TEST(MatchesSymbolAnyTest, AsExpected) {
  EXPECT_FALSE(
      MatchesSymbolAny("test_1", {SymbolSearchPattern{SymbolMatchType::kExact, "test"},
                                  SymbolSearchPattern{SymbolMatchType::kExact, "test_2"}}));
  EXPECT_TRUE(MatchesSymbolAny("test_1", {SymbolSearchPattern{SymbolMatchType::kExact, "test"},
                                          SymbolSearchPattern{SymbolMatchType::kSubstr, "test"}}));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
