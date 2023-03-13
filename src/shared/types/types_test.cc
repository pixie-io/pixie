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

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace types {

template <class T>
class FixedSizedValueTypesOperatorTest : public ::testing::Test {};

using TestedTypes = ::testing::Types<Int64Value, Float64Value, Time64NSValue>;
TYPED_TEST_SUITE(FixedSizedValueTypesOperatorTest, TestedTypes);

TYPED_TEST(FixedSizedValueTypesOperatorTest, serdes_test) {
  auto val1 = TypeParam(1000);
  auto val2 = TypeParam(0);
  PX_CHECK_OK(val2.Deserialize(val1.Serialize()));
  EXPECT_EQ(val1, val2);
}

TYPED_TEST(FixedSizedValueTypesOperatorTest, lt_operator) {
  EXPECT_LT(TypeParam(999), Time64NSValue(1000));
  EXPECT_LT(TypeParam(999), 1000);
  EXPECT_LT(TypeParam(999), 1000LL);
  EXPECT_LT(TypeParam(999), 1000.0f);
  EXPECT_LT(TypeParam(999), 1000.0L);
  EXPECT_LT(TypeParam(999), Int64Value(1000));
  EXPECT_LT(TypeParam(999), Float64Value(1000));

  EXPECT_FALSE(TypeParam(1000) < Time64NSValue(1000));  // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) < 1000);                 // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) < 1000LL);               // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) < 1000.0f);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) < 1000.0L);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) < Int64Value(1000));     // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) < Float64Value(1000));   // NOLINT(readability/check)
}

TYPED_TEST(FixedSizedValueTypesOperatorTest, le_operator) {
  EXPECT_LE(TypeParam(1000), Time64NSValue(1000));
  EXPECT_LE(TypeParam(1000), 1000);
  EXPECT_LE(TypeParam(1000), 1000LL);
  EXPECT_LE(TypeParam(1000), 1000.0f);
  EXPECT_LE(TypeParam(1000), 1000.0L);
  EXPECT_LE(TypeParam(1000), Int64Value(1000));
  EXPECT_LE(TypeParam(1000), Float64Value(1000));

  EXPECT_FALSE(TypeParam(1001) <= Time64NSValue(1000));  // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) <= 1000);                 // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) <= 1000LL);               // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) <= 1000.0f);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) <= 1000.0L);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) <= Int64Value(1000));     // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) <= Float64Value(1000));   // NOLINT(readability/check)
}

TYPED_TEST(FixedSizedValueTypesOperatorTest, gt_operator) {
  EXPECT_GT(TypeParam(1001), Time64NSValue(1000));
  EXPECT_GT(TypeParam(1001), 1000);
  EXPECT_GT(TypeParam(1001), 1000LL);
  EXPECT_GT(TypeParam(1001), 1000.0f);
  EXPECT_GT(TypeParam(1001), 1000.0L);
  EXPECT_GT(TypeParam(1001), Int64Value(1000));
  EXPECT_GT(TypeParam(1001), Float64Value(1000));

  EXPECT_FALSE(TypeParam(1000) > Time64NSValue(1000));  // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) > 1000);                 // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) > 1000LL);               // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) > 1000.0f);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) > 1000.0L);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) > Int64Value(1000));     // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) > Float64Value(1000));   // NOLINT(readability/check)
}

TYPED_TEST(FixedSizedValueTypesOperatorTest, g1_operator) {
  EXPECT_GE(TypeParam(1000), Time64NSValue(1000));
  EXPECT_GE(TypeParam(1000), 1000);
  EXPECT_GE(TypeParam(1000), 1000LL);
  EXPECT_GE(TypeParam(1000), 1000.0f);
  EXPECT_GE(TypeParam(1000), 1000.0L);
  EXPECT_GE(TypeParam(1000), Int64Value(1000));
  EXPECT_GE(TypeParam(1000), Float64Value(1000));

  EXPECT_FALSE(TypeParam(999) >= Time64NSValue(1000));  // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(999) >= 1000);                 // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(999) >= 1000LL);               // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(999) >= 1000.0f);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(999) >= 1000.0L);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(999) >= Int64Value(1000));     // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(999) >= Float64Value(1000));   // NOLINT(readability/check)
}

TYPED_TEST(FixedSizedValueTypesOperatorTest, eq_operator) {
  EXPECT_EQ(TypeParam(1000), Time64NSValue(1000));
  EXPECT_EQ(TypeParam(1000), 1000);
  EXPECT_EQ(TypeParam(1000), 1000LL);
  EXPECT_EQ(TypeParam(1000), 1000.0f);
  EXPECT_EQ(TypeParam(1000), 1000.0L);
  EXPECT_EQ(TypeParam(1000), Int64Value(1000));

  EXPECT_FALSE(TypeParam(1001) == Time64NSValue(1000));  // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) == 1000);                 // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) == 1000LL);               // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) == 1000.0f);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) == 1000.0L);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1001) == Int64Value(1000));     // NOLINT(readability/check)
}

TYPED_TEST(FixedSizedValueTypesOperatorTest, neq_operator) {
  EXPECT_NE(TypeParam(1001), Time64NSValue(1000));
  EXPECT_NE(TypeParam(1001), 1000);
  EXPECT_NE(TypeParam(1001), 1000LL);
  EXPECT_NE(TypeParam(1001), 1000.0f);
  EXPECT_NE(TypeParam(1001), 1000.0L);
  EXPECT_NE(TypeParam(1001), Int64Value(1000));

  EXPECT_FALSE(TypeParam(1000) != Time64NSValue(1000));  // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) != 1000);                 // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) != 1000LL);               // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) != 1000.0f);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) != 1000.0L);              // NOLINT(readability/check)
  EXPECT_FALSE(TypeParam(1000) != Int64Value(1000));     // NOLINT(readability/check)
}

TEST(UInt128Value, eq_operator) {
  EXPECT_EQ(UInt128Value(absl::MakeUint128(100, 200)), absl::MakeUint128(100, 200));
  EXPECT_EQ(UInt128Value(absl::MakeUint128(0, 200)), 200);
}

TEST(UInt128Value, neq_operator) {
  EXPECT_NE(UInt128Value(absl::MakeUint128(100, 200)), absl::MakeUint128(200, 200));
  EXPECT_NE(UInt128Value(absl::MakeUint128(0, 200)), 300);
}

TEST(UInt128Value, high_low_funcs) {
  auto v1 = UInt128Value(absl::MakeUint128(100, 200));
  EXPECT_EQ(200, v1.Low64());
  EXPECT_EQ(100, v1.High64());
}

}  // namespace types
}  // namespace px
