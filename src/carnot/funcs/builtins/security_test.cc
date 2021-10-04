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

#include "src/carnot/funcs/builtins/security.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(MathOps, basic_string_add_test) {
  auto udf_tester =
      udf::UDFTester<AddUDF<types::StringValue, types::StringValue, types::StringValue>>();
  udf_tester.ForInput("a", "b").Expect("ab");
}
}  // namespace builtins
}  // namespace carnot
}  // namespace px
