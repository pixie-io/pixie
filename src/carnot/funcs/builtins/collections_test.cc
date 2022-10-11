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

TEST(CollectionsTest, CanSerializeDeserialize_Float64) {
  auto uda_tester = udf::UDATester<AnyUDA<types::Float64Value>>();
  std::vector<types::Float64Value> vals = {
      1.234, 2.442, 1.04, 5.322, 6.333,
  };

  for (const auto& val : vals) {
    uda_tester.ForInput(val);
  }

  auto any_uda = AnyUDA<types::Float64Value>();
  EXPECT_NE(uda_tester.Result(), any_uda.Finalize(nullptr));
  ASSERT_OK(any_uda.Deserialize(nullptr, uda_tester.Serialize()));

  EXPECT_EQ(uda_tester.Result(), any_uda.Finalize(nullptr));
}

TEST(CollectionsTest, CanSerializeDeserialize_String) {
  auto uda_tester = udf::UDATester<AnyUDA<types::StringValue>>();
  std::vector<types::StringValue> vals = {
      "hi", "hello", "foo", "bar", "baz",
  };

  for (const auto& val : vals) {
    uda_tester.ForInput(val);
  }

  auto any_uda = AnyUDA<types::StringValue>();
  EXPECT_NE(uda_tester.Result(), any_uda.Finalize(nullptr));
  ASSERT_OK(any_uda.Deserialize(nullptr, uda_tester.Serialize()));

  EXPECT_EQ(uda_tester.Result(), any_uda.Finalize(nullptr));
}

TEST(CollectionsTest, CanSerializeDeserialize_UInt128) {
  auto uda_tester = udf::UDATester<AnyUDA<types::UInt128Value>>();
  std::vector<types::UInt128Value> vals = {
      {1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10},
  };

  for (const auto& val : vals) {
    uda_tester.ForInput(val);
  }

  auto any_uda = AnyUDA<types::UInt128Value>();
  EXPECT_NE(uda_tester.Result(), any_uda.Finalize(nullptr));
  ASSERT_OK(any_uda.Deserialize(nullptr, uda_tester.Serialize()));

  EXPECT_EQ(uda_tester.Result(), any_uda.Finalize(nullptr));
}

TEST(CollectionsTest, CanSerializeDeserialize_Float) {
  auto uda_tester = udf::UDATester<AnyUDA<types::Float64Value>>();
  std::vector<types::Float64Value> vals = {
      0.1, 0.2, 0.3, 0.4, 0.5,
  };

  for (const auto& val : vals) {
    uda_tester.ForInput(val);
  }

  auto any_uda = AnyUDA<types::Float64Value>();
  EXPECT_NE(uda_tester.Result(), any_uda.Finalize(nullptr));
  ASSERT_OK(any_uda.Deserialize(nullptr, uda_tester.Serialize()));

  EXPECT_EQ(uda_tester.Result(), any_uda.Finalize(nullptr));
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
