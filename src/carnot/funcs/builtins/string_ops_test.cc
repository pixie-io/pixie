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

#include "src/carnot/funcs/builtins/string_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(StringOps, basic_string_contains_test) {
  auto udf_tester = udf::UDFTester<ContainsUDF>();
  udf_tester.ForInput("apple", "pl").Expect(true);
  udf_tester.ForInput("apple", "z").Expect(false);
}

TEST(StringOps, basic_string_length_test) {
  auto udf_tester = udf::UDFTester<LengthUDF>();
  udf_tester.ForInput("").Expect(0);
  udf_tester.ForInput("apple").Expect(5);
}

TEST(StringOps, basic_string_find_test) {
  auto udf_tester = udf::UDFTester<FindUDF>();
  udf_tester.ForInput("pixielabs", "xie").Expect(2);
  udf_tester.ForInput("pixielabs", "hello").Expect(-1);
}

TEST(StringOps, basic_string_substr_test) {
  auto udf_tester = udf::UDFTester<SubstringUDF>();
  udf_tester.ForInput("pixielabs", 3, 4).Expect("iela");
  udf_tester.ForInput("pixielabs", 5, 10).Expect("labs");
  udf_tester.ForInput("pixielabs", 5, 0).Expect("");
}

TEST(StringOps, substring_edge_cases) {
  auto udf_tester = udf::UDFTester<SubstringUDF>();
  // Start pos > length.
  udf_tester.ForInput("pixielabs", 9, 20).Expect("");
  // -1 starting position.
  udf_tester.ForInput("pixielabs", -1, 2).Expect("");
  // -1 length.
  udf_tester.ForInput("pixielabs", 0, -1).Expect("");
}

TEST(StringOps, basic_string_tolower_test) {
  auto udf_tester = udf::UDFTester<ToLowerUDF>();
  udf_tester.ForInput("pIXiE").Expect("pixie");
}

TEST(StringOps, basic_string_toupper_test) {
  auto udf_tester = udf::UDFTester<ToUpperUDF>();
  udf_tester.ForInput("pIXiE").Expect("PIXIE");
}

TEST(StringOps, basic_string_trim_test) {
  auto udf_tester = udf::UDFTester<TrimUDF>();
  udf_tester.ForInput("   ").Expect("");
  udf_tester.ForInput(" pixieLabs ").Expect("pixieLabs");
  udf_tester.ForInput("pixie").Expect("pixie");
  udf_tester.ForInput("p   ixie").Expect("p   ixie");
}

TEST(StringOps, basic_string_strip_prefix) {
  auto udf_tester = udf::UDFTester<StripPrefixUDF>();
  udf_tester.ForInput("sock-shop/", "sock-shop/carts").Expect("carts");
  udf_tester.ForInput("sock-shop/carts", "sock-shop/carts").Expect("");
  udf_tester.ForInput("sock-shop/carts123", "sock-shop/carts").Expect("sock-shop/carts");
}

TEST(StringOps, HexToASCII) {
  auto udf_tester = udf::UDFTester<HexToASCII>();
  udf_tester.ForInput("36623330303663622d393632612d343030302d616235652d333636383564616634383030")
      .Expect("6b3006cb-962a-4000-ab5e-36685daf4800");
  // Missing last nibble.
  udf_tester.ForInput("333").Expect("");
}

TEST(StringOps, BytesToHex) {
  auto udf_tester = udf::UDFTester<BytesToHex>();
  udf_tester.ForInput("\x01\x02\x03").Expect(R"(\x01\x02\x03)");
  udf_tester.ForInput("abc").Expect(R"(\x61\x62\x63)");
}

TEST(StringOps, StringToInt) {
  auto udf_tester = udf::UDFTester<StringToIntUDF>();
  udf_tester.ForInput("1234", -1).Expect(1234);
  udf_tester.ForInput("-1234", -1).Expect(-1234);
  udf_tester.ForInput("not an int", -1).Expect(-1);
  udf_tester.ForInput("not an int", 1234).Expect(1234);
  udf_tester.ForInput("+1111111", -1).Expect(1111111);
}

TEST(StringOps, IntToString) {
  auto udf_tester = udf::UDFTester<IntToStringUDF>();
  udf_tester.ForInput(1234).Expect("1234");
  udf_tester.ForInput(-1234).Expect("-1234");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
