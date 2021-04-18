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
#include <rapidjson/document.h>

#include "src/carnot/funcs/builtins/math_sketches.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(MathSketches, quantiles_float64) {
  auto uda_tester = udf::UDATester<QuantilesUDA<types::Float64Value>>();
  // This test mostly makes sure that the UDA code runs and produces results.
  // Tdigest is heavily unit tested to be statistically correct.
  auto res = uda_tester.ForInput(1.234)
                 .ForInput(2.442)
                 .ForInput(1.04)
                 .ForInput(5.322)
                 .ForInput(6.333)
                 .Result();

  rapidjson::Document d;
  d.Parse(res.data());
  EXPECT_DOUBLE_EQ(d["p01"].GetDouble(), 1.04);
  EXPECT_DOUBLE_EQ(d["p10"].GetDouble(), 1.04);
  EXPECT_DOUBLE_EQ(d["p50"].GetDouble(), 2.442);
  EXPECT_DOUBLE_EQ(d["p90"].GetDouble(), 6.333);
  EXPECT_DOUBLE_EQ(d["p99"].GetDouble(), 6.333);
}

TEST(MathSketches, quantiles_int64) {
  auto uda_tester = udf::UDATester<QuantilesUDA<types::Float64Value>>();
  // This test mostly makes sure that the UDA code runs and produces results.
  // Tdigest is heavily unit tested to be statistically correct.
  auto res = uda_tester.ForInput(1)
                 .ForInput(2)
                 .ForInput(2)
                 .ForInput(1)
                 .ForInput(1)
                 .ForInput(5)
                 .ForInput(6)
                 .Result();

  rapidjson::Document d;
  d.Parse(res.data());
  EXPECT_DOUBLE_EQ(d["p01"].GetDouble(), 1);
  EXPECT_DOUBLE_EQ(d["p10"].GetDouble(), 1);
  EXPECT_DOUBLE_EQ(d["p50"].GetDouble(), 2);
  EXPECT_DOUBLE_EQ(d["p90"].GetDouble(), 5.7999999999999998);
  EXPECT_DOUBLE_EQ(d["p99"].GetDouble(), 6);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
