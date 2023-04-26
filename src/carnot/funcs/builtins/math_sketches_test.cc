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

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <gtest/gtest.h>

#include "src/carnot/funcs/builtins/math_sketches.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

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

TEST(MathSketches, quantiles_serialize) {
  auto uda_tester = udf::UDATester<QuantilesUDA<types::Float64Value>>();
  auto serialized = uda_tester.ForInput(1).Serialize();
  rapidjson::Document d;
  d.Parse(serialized.data());
  const auto& processed = d[QuantilesUDA<types::Float64Value>::kProcessedKey];
  EXPECT_TRUE(processed.IsArray());
  EXPECT_EQ(0, processed.GetArray().Size());
  const auto& unprocessed = d[QuantilesUDA<types::Float64Value>::kUnprocessedKey];
  EXPECT_EQ(1, unprocessed.GetArray().Size());
  const auto& first_centroid = unprocessed.GetArray()[0].GetArray();
  EXPECT_EQ(2, first_centroid.Size());
  EXPECT_EQ(1.0, first_centroid[0].GetDouble());
  EXPECT_EQ(1.0, first_centroid[1].GetDouble());
  // The other members of the serialized json are opaque internals to tdigest, so we don't test
  // those here.
}

TEST(MathSketches, quantiles_serde) {
  auto uda_tester = udf::UDATester<QuantilesUDA<types::Float64Value>>();
  auto res_before_serde = uda_tester.ForInput(1)
                              .ForInput(2)
                              .ForInput(2)
                              .ForInput(1)
                              .ForInput(1)
                              .ForInput(5)
                              .ForInput(6)
                              .Result();
  auto serialized = uda_tester.Serialize();
  auto new_uda_tester = udf::UDATester<QuantilesUDA<types::Float64Value>>();
  EXPECT_OK(new_uda_tester.Deserialize(serialized));
  auto res_after_serde = new_uda_tester.Result();
  EXPECT_EQ(res_before_serde, res_after_serde);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
