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

#include <benchmark/benchmark.h>

#include <iostream>
#include <regex>
#include <vector>

#include "src/common/base/base.h"

constexpr std::string_view kSeparator = ";;;";

std::vector<std::string> GenerateData(std::string* expected_value) {
  constexpr int kNumElements = 50;
  constexpr int kElementLength = 100;

  expected_value->clear();

  uint32_t seed = 37;

  std::vector<std::string> data;
  for (int i = 0; i < kNumElements; ++i) {
    std::string s(kElementLength, 'a' + rand_r(&seed) % 26);
    data.push_back(s);
    *expected_value += s;
    *expected_value += kSeparator;
  }

  return data;
}

size_t ComputeFinalSize(const std::vector<std::string>& strings) {
  size_t final_size = 0;
  for (const auto& s : strings) {
    final_size += s.size() + kSeparator.size();
  }
  return final_size;
}

// NOLINTNEXTLINE(runtime/references)
static void BM_str_append(benchmark::State& state) {
  std::string expected_value;
  std::vector<std::string> strings = GenerateData(&expected_value);

  for (auto _ : state) {
    std::string out;

    for (const auto& s : strings) {
      out += s;
      out += kSeparator;
    }

    DCHECK_EQ(out, expected_value);
    benchmark::DoNotOptimize(out);
  }
}

// NOLINTNEXTLINE(runtime/references)
static void BM_str_append_with_reserve(benchmark::State& state) {
  std::string expected_value;
  std::vector<std::string> strings = GenerateData(&expected_value);

  for (auto _ : state) {
    std::string out;
    out.reserve(ComputeFinalSize(strings));

    for (const auto& s : strings) {
      out += s;
      out += kSeparator;
    }

    DCHECK_EQ(out, expected_value);
    benchmark::DoNotOptimize(out);
  }
}

// NOLINTNEXTLINE(runtime/references)
static void BM_absl_str_append(benchmark::State& state) {
  std::string expected_value;
  std::vector<std::string> strings = GenerateData(&expected_value);

  for (auto _ : state) {
    std::string out;

    for (const auto& s : strings) {
      absl::StrAppend(&out, s, kSeparator);
    }

    DCHECK_EQ(out, expected_value);
    benchmark::DoNotOptimize(out);
  }
}

// NOLINTNEXTLINE(runtime/references)
static void BM_absl_str_append_with_reserve(benchmark::State& state) {
  std::string expected_value;
  std::vector<std::string> strings = GenerateData(&expected_value);

  for (auto _ : state) {
    std::string out;
    out.reserve(ComputeFinalSize(strings));

    for (const auto& s : strings) {
      absl::StrAppend(&out, s, kSeparator);
    }

    DCHECK_EQ(out, expected_value);
    benchmark::DoNotOptimize(out);
  }
}

// NOLINTNEXTLINE(runtime/references)
static void BM_absl_str_join(benchmark::State& state) {
  std::string expected_value;
  std::vector<std::string> strings = GenerateData(&expected_value);

  for (auto _ : state) {
    std::string out;

    out = absl::StrJoin(strings, kSeparator);
    out += kSeparator;

    DCHECK_EQ(out, expected_value);
    benchmark::DoNotOptimize(out);
  }
}

// NOLINTNEXTLINE(runtime/references)
static void BM_absl_str_join_with_reserve(benchmark::State& state) {
  std::string expected_value;
  std::vector<std::string> strings = GenerateData(&expected_value);

  for (auto _ : state) {
    std::string out;
    out.reserve(ComputeFinalSize(strings));

    out = absl::StrJoin(strings, kSeparator);
    out += kSeparator;

    DCHECK_EQ(out, expected_value);
    benchmark::DoNotOptimize(out);
  }
}

BENCHMARK(BM_str_append);
BENCHMARK(BM_str_append_with_reserve);
BENCHMARK(BM_absl_str_append);
BENCHMARK(BM_absl_str_append_with_reserve);
BENCHMARK(BM_absl_str_join);
BENCHMARK(BM_absl_str_join_with_reserve);
