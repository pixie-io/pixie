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

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <vector>

#include "src/carnot/funcs/builtins/conditionals.h"
#include "src/carnot/udf/test_utils.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(ConditionalsTest, SelectUDF) {
  auto udf_tester = udf::UDFTester<SelectUDF<types::Int64Value>>();
  udf_tester.ForInput(false, 20, 21).Expect(21);
  udf_tester.ForInput(true, 20, 21).Expect(20);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
