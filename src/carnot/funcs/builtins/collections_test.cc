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

#include "src/carnot/funcs/builtins/collections.h"
#include "src/carnot/udf/test_utils.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(CollectionsTest, AnyUDA) {
  auto uda_tester = udf::UDATester<AnyUDA<types::Float64Value>>();
  std::vector<types::Float64Value> vals = {
      1.234, 2.442, 1.04, 5.322, 6.333,
  };

  for (const auto& val : vals) {
    uda_tester.ForInput(val);
  }

  EXPECT_THAT(vals, ::testing::Contains(uda_tester.Result()));
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
