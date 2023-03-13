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

#include "src/carnot/exec/row_tuple.h"

namespace px {
namespace carnot {
namespace exec {
std::vector<types::DataType> types_single = {
    types::DataType::INT64,
};

std::vector<types::DataType> types_variable1 = {
    types::DataType::BOOLEAN,
    types::DataType::INT64,
    types::DataType::FLOAT64,
    types::DataType::STRING,
};

std::vector<types::DataType> types_variable2 = {
    types::DataType::INT64,
    types::DataType::STRING,
    types::DataType::INT64,
    types::DataType::STRING,
};

class RowTupleTest : public ::testing::Test {
  void SetUp() override {
    rt1_.SetValue(0, types::BoolValue(false));
    rt1_.SetValue(1, types::Int64Value(1));
    rt1_.SetValue(2, types::Float64Value(2.0));
    rt1_.SetValue(3, types::StringValue("ABC"));

    rt2_.SetValue(0, types::BoolValue(false));
    rt2_.SetValue(1, types::Int64Value(1));
    rt2_.SetValue(2, types::Float64Value(2.0));
    rt2_.SetValue(3, types::StringValue("ABC"));
  }

 protected:
  RowTuple rt0_{&types_single};
  RowTuple rt1_{&types_variable1};
  RowTuple rt2_{&types_variable1};
  RowTuple rt3_{&types_variable2};
  RowTuple rt4_{&types_variable2};
};

TEST_F(RowTupleTest, single_value) {
  rt0_.SetValue(0, types::Int64Value(123123));
  EXPECT_EQ(123123, rt0_.GetValue<types::Int64Value>(0).val);
}

TEST_F(RowTupleTest, check_equality_func_with_same_types) {
  EXPECT_TRUE(rt1_ == rt2_);
  EXPECT_EQ(rt1_.Hash(), rt2_.Hash());
}

TEST_F(RowTupleTest, check_getter) {
  EXPECT_EQ(1, rt1_.GetValue<types::Int64Value>(1).val);
  EXPECT_EQ("ABC", rt1_.GetValue<types::StringValue>(3));
}

TEST_F(RowTupleTest, check_equality_func_with_same_types_same_after_reset) {
  rt2_.Reset();
  rt2_.SetValue(0, types::BoolValue(false));
  rt2_.SetValue(1, types::Int64Value(1));
  rt2_.SetValue(2, types::Float64Value(2.0));
  rt2_.SetValue(3, types::StringValue("ABC"));
  EXPECT_TRUE(rt1_ == rt2_);
}

TEST_F(RowTupleTest, check_equality_func_with_same_types_different_after_reset) {
  rt2_.Reset();
  rt2_.SetValue(0, types::BoolValue(false));
  rt2_.SetValue(1, types::Int64Value(1));
  rt2_.SetValue(2, types::Float64Value(2.0));
  // Added D.
  rt2_.SetValue(3, types::StringValue("ABCD"));
  EXPECT_FALSE(rt1_ == rt2_);
  EXPECT_NE(rt1_.Hash(), rt2_.Hash());
}

using RowTupleDeathTest = RowTupleTest;

TEST_F(RowTupleDeathTest, read_wrong_type) {
  EXPECT_DEBUG_DEATH(rt1_.GetValue<types::Int64Value>(0), ".*types.*");
}

TEST_F(RowTupleDeathTest, should_debug_die_on_different_types) {
  EXPECT_DEBUG_DEATH(PX_UNUSED((rt1_ == rt3_)), ".*types.*");
}

TEST_F(RowTupleDeathTest, should_debug_die_on_bad_order) {
  rt3_.SetValue(0, types::Int64Value(1));
  rt3_.SetValue(1, types::StringValue("abc"));
  rt3_.SetValue(2, types::Int64Value(1));
  rt3_.SetValue(3, types::StringValue("def"));

  rt4_.SetValue(0, types::Int64Value(1));
  rt4_.SetValue(3, types::StringValue("def"));
  rt4_.SetValue(1, types::StringValue("abc"));
  rt4_.SetValue(2, types::Int64Value(1));

  EXPECT_DEBUG_DEATH(PX_UNUSED((rt3_ == rt4_)), ".*write ordering mismatch.*");
  EXPECT_DEBUG_DEATH(rt4_.Hash(), ".*write ordering mismatch.*");
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
