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

namespace px {
namespace carnot {
namespace builtins {

TEST(StringOps, basic_regex_match) {
  auto udf_tester = udf::UDFTester<RegexMatchUDF>();
  udf_tester.Init("abcd.*").ForInput("abcdefg").Expect(true);
  udf_tester.Init("abcd.*").ForInput("abce").Expect(false);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
