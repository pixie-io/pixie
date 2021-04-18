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

#include "src/common/testing/line_diff.h"

#include <gmock/gmock.h>  // IWYU pragma: export
#include <gtest/gtest.h>  // IWYU pragma: export

#include "src/common/base/base.h"

namespace px {
namespace testing {

using ::testing::StrEq;

TEST(DiffingTest, CommonCases) {
  const std::string lhs =
      R"(
a
f
c
f
e)";
  const std::string rhs =
      R"(
a
b
c
e)";
  EXPECT_THAT(DiffLines(lhs, rhs), StrEq("  \n"
                                         R"(  a
l:f
r:b
  c
l:f
  e)"));
  EXPECT_THAT(DiffLines(lhs, rhs, DiffPolicy::kIgnoreBlankLines), StrEq(
                                                                      R"(  a
l:f
r:b
  c
l:f
  e)"));
}

TEST(DiffingTest, EmptyStrings) {
  EXPECT_THAT(DiffLines("", R"(a
b)"),
              StrEq(R"(l:
r:a
r:b)"));
  EXPECT_THAT(DiffLines("", R"(a
b)",
                        DiffPolicy::kIgnoreBlankLines),
              StrEq(R"(r:a
r:b)"));

  EXPECT_THAT(DiffLines(R"(a
b)",
                        ""),
              StrEq(R"(l:a
l:b
r:)"));
  EXPECT_THAT(DiffLines(R"(a
b)",
                        "", DiffPolicy::kIgnoreBlankLines),
              StrEq(R"(l:a
l:b)"));
}

}  // namespace testing
}  // namespace px
