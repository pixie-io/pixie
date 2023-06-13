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

#include "src/carnot/funcs/builtins/pprof_ops.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "src/carnot/udf/test_utils.h"
#include "src/shared/pprof/pprof.h"

namespace px {
namespace carnot {
namespace builtins {

using px::shared::DeserializePProfProfile;
using px::shared::PProfProfile;

namespace {
constexpr int64_t profiler_period_ms = 11;
}

TEST(PProf, profiling_rows_to_pprof_test) {
  // Raw stack trace string and count input data.
  // Note, there are duplicated stack traces.
  const std::vector<std::pair<std::string, uint64_t>> input = {
      {"foo;bar;baz", 1},
      {"foo;bar;baz", 2},
      {"foo;bar;qux", 3},
      {"foo;bar", 4},
      {"foo;bar", 5},
      {"main;compute;map;reduce", 6},
      {"main;compute;map;reduce", 7},
      {"main;compute;map;reduce", 8},
  };

  // The expected output as a histogram (string=>count map).
  const absl::flat_hash_map<std::string, uint64_t> expected = {
      {"foo;bar;baz", 1 + 2},
      {"foo;bar;qux", 3},
      {"foo;bar", 4 + 5},
      {"main;compute;map;reduce", 6 + 7 + 8},
  };

  // Create our UDA tester.
  auto pprof_uda_tester = udf::UDATester<CreatePProfRowAggregate>();

  // Feed the input to the tester.
  for (const auto& [stack_trace, count] : input) {
    pprof_uda_tester.ForInput(stack_trace, count, profiler_period_ms);
  }

  // Get the result.
  const std::string result = pprof_uda_tester.Result();

  // Parse the result.
  PProfProfile pprof;
  EXPECT_TRUE(pprof.ParseFromString(result));

  // Deserialize the parsed pprof into a histo.
  const auto actual = DeserializePProfProfile(pprof);

  // Expect the deserialized result to be equal to our expected value.
  EXPECT_EQ(actual, expected);
}

TEST(PProf, pprof_merge_test) {
  // Raw stack trace string and count input data.
  // Note, there are duplicated stack traces.
  const std::vector<std::pair<std::string, uint64_t>> input_a = {
      {"foo;bar;baz", 1},
      {"foo;bar;qux", 2},
      {"main;compute;map;reduce", 3},
  };
  const std::vector<std::pair<std::string, uint64_t>> input_b = {
      {"foo;bar;baz", 5},
      {"foo;bar;baz", 5},
      {"foo;bar;qux", 10},
      {"foo;bar;qux", 10},
      {"main;compute;map;reduce", 15},
      {"main;compute;map;reduce", 15},
  };

  // The expected output as a histogram (string=>count map).
  const absl::flat_hash_map<std::string, uint64_t> expected = {
      {"foo;bar;baz", 11},
      {"foo;bar;qux", 22},
      {"main;compute;map;reduce", 33},
  };

  auto pprof_uda_tester_a = udf::UDATester<CreatePProfRowAggregate>();
  auto pprof_uda_tester_b = udf::UDATester<CreatePProfRowAggregate>();
  auto pprof_uda_tester_merge = udf::UDATester<CreatePProfRowAggregate>();

  // Feed the A inputs to the tester_a.
  for (const auto& [stack_trace, count] : input_a) {
    pprof_uda_tester_a.ForInput(stack_trace, count, profiler_period_ms);
  }

  // Feed the B inputs to the tester_b.
  for (const auto& [stack_trace, count] : input_b) {
    pprof_uda_tester_b.ForInput(stack_trace, count, profiler_period_ms);
  }

  // Merge A & B UDAs into the final "merge" UDA.
  EXPECT_OK(pprof_uda_tester_merge.Deserialize(pprof_uda_tester_a.Serialize()));
  EXPECT_OK(pprof_uda_tester_merge.Deserialize(pprof_uda_tester_b.Serialize()));

  // Get the result.
  const std::string result = pprof_uda_tester_merge.Result();

  // Parse the result.
  PProfProfile pprof;
  EXPECT_TRUE(pprof.ParseFromString(result));

  // Deserialize the parsed pprof into a histo.
  const auto actual = DeserializePProfProfile(pprof);

  // Expect the deserialized result to be equal to our expected value.
  EXPECT_EQ(actual, expected);
}

TEST(PProf, uda_fails_with_multiple_sample_periods) {
  // Create our UDA tester.
  auto pprof_uda_tester = udf::UDATester<CreatePProfRowAggregate>();

  // Input two stack traces, but with different profiling sample periods.
  pprof_uda_tester.ForInput("foo;bar;baz", 1, profiler_period_ms);
  pprof_uda_tester.ForInput("foo;bar;qux", 2, profiler_period_ms + 1);

  // Get the result.
  const std::string result = pprof_uda_tester.Result();

  // Parse the result: this should fail because we used multiple sample periods above.
  PProfProfile pprof;
  EXPECT_FALSE(pprof.ParseFromString(result));
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
