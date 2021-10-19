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

#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

#include "src/carnot/funcs/builtins/regex_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/testing/testing.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(RegexOps, basic_regex_match) {
  auto udf_tester = udf::UDFTester<RegexMatchUDF>();
  udf_tester.Init("abcd.*").ForInput("abcdefg").Expect(true);
  udf_tester.Init("abcd.*").ForInput("abce").Expect(false);
}

TEST(RegexOps, regex_match_rules) {
  auto udf_tester =
      udf::UDFTester<MatchRegexRule>();
  // Value matches regex rule.
  udf_tester.Init(
    "{\"onpointerenter_event\":\".*[oO][nN][pP][oO][iI][nN][tT][eE][rR][eE][nN][tT][eE][rR].*\"}"
  ).ForInput(
    "UPDATE courses SET name = '<a/+/OnpOinteRENtER+=+a=prompt,a()%0dx>v3dm0s ' WHERE id = 2"
  ).Expect("onpointerenter_event");
  // Value does not match regex rule.
  udf_tester.Init(
    "{\"onpointerenter_event\":\".*[oO][nN][pP][oO][iI][nN][tT][eE][rR][eE][nN][tT][eE][rR].*\"}"
  ).ForInput(
    "UPDATE courses SET name = 'foo' WHERE id = 2"
  ).Expect("");
  // Regex rules is not a valid json.
  EXPECT_NOT_OK(
    udf_tester.Init(
      ".*[oO][nN][pP][oO][iI][nN][tT][eE][rR][eE][nN][tT][eE][rR].*"
    ).ForInput(
      "UPDATE courses SET name = 'foo' WHERE id = 2"
    )
  );
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
