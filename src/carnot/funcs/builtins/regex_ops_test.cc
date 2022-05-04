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
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace builtins {

constexpr char kMultiLine[] = R"str(
abcd
1234
testtest
)str";

TEST(RegexOps, basic_regex_match) {
  auto udf_tester = udf::UDFTester<RegexMatchUDF>();
  udf_tester.Init("abcd.*").ForInput("abcdefg").Expect(true);
  udf_tester.Init("abcd.*").ForInput("abce").Expect(false);
}

TEST(RegexOps, regex_match_new_line) {
  auto udf_tester = udf::UDFTester<RegexMatchUDF>();
  udf_tester.Init(".*").ForInput("abcd\nefg").Expect(true);
}

TEST(RegexOps, invalid_regex_match) {
  auto udf_tester = udf::UDFTester<RegexMatchUDF>();
  udf_tester.Init(R"regex(\K)regex").ForInput(kMultiLine).Expect(false);
}

TEST(RegexOps, basic_regex_replace) {
  auto udf_tester = udf::UDFTester<RegexReplaceUDF>();
  udf_tester.Init("abc").ForInput("1abc 2abcd\t\n3abcd", "__").Expect("1__ 2__d\t\n3__d");
  udf_tester.Init("\n").ForInput(kMultiLine, "\t").Expect("\tabcd\t1234\ttesttest\t");
  udf_tester.Init("\n").ForInput(kMultiLine, "").Expect("abcd1234testtest");
}

TEST(RegexOps, regex_replace_w_groups) {
  auto udf_tester = udf::UDFTester<RegexReplaceUDF>();
  // Numbered group
  udf_tester.Init(R"regex(([0-9]+)\w+)regex")
      .ForInput("123abcd 000chars 0asdfasdfasdfasdfasdfasdfasdf", R"regex(\1word)regex")
      .Expect("123word 000word 0word");
}

TEST(RegexOps, invalid_regex_replace) {
  auto udf_tester = udf::UDFTester<RegexReplaceUDF>();
  udf_tester.Init(R"regex(\K)regex")
      .ForInput(kMultiLine, "")
      .Expect("Invalid regex expr: invalid escape sequence: \\K");

  // Named groups unsupported.
  udf_tester.Init(R"regex((?P<numbers>[0-9]+)\w+)regex")
      .ForInput("123abcd 000chars 0asdfasdfasdfasdfasdfasdfasdf", R"regex(\g<numbers>word)regex")
      .Expect(
          "Invalid regex in substitution string: Rewrite schema error: '\\' must be followed by "
          "a digit or '\\'.");
}

TEST(RegexOps, regex_match_rules) {
  auto udf_tester = udf::UDFTester<MatchRegexRule>();
  // Value matches regex rule.
  udf_tester.Init("{\"onpointerenter_event\":\"(?i).*onpointerenter.*\"}")
      .ForInput(
          "UPDATE courses SET name = '<a/+/OnpOinteRENtER+=+a=prompt,a()%0dx>v3dm0s ' WHERE id = 2")
      .Expect("onpointerenter_event");
  // Value does not match regex rule.
  udf_tester.Init("{\"onpointerenter_event\":\"(?i).*onpointerenter.*\"}")
      .ForInput("UPDATE courses SET name = 'foo' WHERE id = 2")
      .Expect("");
  // Regex rules is not a valid json.
  EXPECT_NOT_OK(MatchRegexRule().Init(nullptr, "(?i).*onpointerenter.*"));
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
