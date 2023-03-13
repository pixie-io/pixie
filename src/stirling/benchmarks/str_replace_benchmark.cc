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

// NOLINTNEXTLINE : runtime/string.
const std::string kOrigStr =
    "This is a ? with multiple placeholders. For example: SELECT ? FROM Table WHERE id=?. This "
    "string could potentially be as long as ? characters.";

const std::vector<std::string_view> kParams = {"string", "name, email", "32423", "10000"};

// NOLINTNEXTLINE : runtime/string.
const std::string kExpectedResult =
    "This is a string with multiple placeholders. For example: SELECT name, email FROM Table WHERE "
    "id=32423. This "
    "string could potentially be as long as 10000 characters.";

// NOLINTNEXTLINE : runtime/references.
static void BM_char_iteration(benchmark::State& state) {
  // Only putting this in one test.
  // Meant to check the input.
  CHECK_EQ(static_cast<size_t>(std::count(kOrigStr.begin(), kOrigStr.end(), '?')), kParams.size());

  for (auto _ : state) {
    size_t pos = 0;
    std::string result = "";
    for (size_t i = 0; i < kOrigStr.size(); ++i) {
      if (kOrigStr[i] == '?') {
        result += kParams[pos];
        pos++;
      } else {
        result += kOrigStr[i];
      }
    }

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_find_and_replace(benchmark::State& state) {
  for (auto _ : state) {
    size_t offset = 0;
    size_t count = 0;
    std::string result = "";

    for (size_t index = kOrigStr.find("?", offset); index != std::string::npos;
         index = kOrigStr.find("?", offset)) {
      absl::StrAppend(&result, kOrigStr.substr(offset, index - offset), kParams[count]);
      count++;
      offset = index + 1;
    }
    result += kOrigStr.substr(offset);

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_find_and_replace_with_reserve(benchmark::State& state) {
  for (auto _ : state) {
    size_t offset = 0;
    size_t count = 0;
    std::string result;

    int final_length = kOrigStr.size();
    for (const auto& param : kParams) {
      final_length += param.length() - 1;
    }
    result.reserve(final_length);

    for (size_t index = kOrigStr.find("?", offset); index != std::string::npos;
         index = kOrigStr.find("?", offset)) {
      absl::StrAppend(&result, kOrigStr.substr(offset, index - offset), kParams[count]);
      count++;
      offset = index + 1;
    }
    result += kOrigStr.substr(offset);

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_find_and_replace_with_reserve_and_shrink(benchmark::State& state) {
  for (auto _ : state) {
    size_t offset = 0;
    size_t count = 0;
    std::string result;

    result.reserve(kOrigStr.size() * 10);

    for (size_t index = kOrigStr.find("?", offset); index != std::string::npos;
         index = kOrigStr.find("?", offset)) {
      absl::StrAppend(&result, kOrigStr.substr(offset, index - offset), kParams[count]);
      count++;
      offset = index + 1;
    }
    result += kOrigStr.substr(offset);

    result.shrink_to_fit();

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_str_replace(benchmark::State& state) {
  for (auto _ : state) {
    size_t offset = 0;
    size_t count = 0;
    std::string result = kOrigStr;

    for (size_t index = result.find("?", offset); index != std::string::npos;
         index = result.find("?", offset)) {
      const auto& param = kParams[count];
      count++;

      result.replace(index, 1, param);
      offset = index + param.length();
    }

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_str_replace_with_reserve(benchmark::State& state) {
  for (auto _ : state) {
    size_t offset = 0;
    size_t count = 0;
    std::string result = kOrigStr;

    int final_length = kOrigStr.size();
    for (const auto& param : kParams) {
      final_length += param.length() - 1;
    }
    result.reserve(final_length);

    for (size_t index = result.find("?", offset); index != std::string::npos;
         index = result.find("?", offset)) {
      const auto& param = kParams[count];
      count++;

      result.replace(index, 1, param);
      offset = index + param.length();
    }

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_regex_replace(benchmark::State& state) {
  const std::regex pattern(R"(\?)");

  for (auto _ : state) {
    std::string result = kOrigStr;

    for (const auto& param : kParams) {
      result = std::regex_replace(result, pattern, std::string(param),
                                  std::regex_constants::format_first_only);
    }

    DCHECK_EQ(result, kExpectedResult);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_char_iteration);
BENCHMARK(BM_find_and_replace);
BENCHMARK(BM_find_and_replace_with_reserve);
BENCHMARK(BM_find_and_replace_with_reserve_and_shrink);
BENCHMARK(BM_str_replace);
BENCHMARK(BM_str_replace_with_reserve);
BENCHMARK(BM_regex_replace);
