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

#include <regex>

#include <benchmark/benchmark.h>

namespace px {

const char* kPodNameMatch = "pod8dbc5577_d0e2_4706_8787_57d52c03ddf2";
const char* kPodNameFail = "pod8dbc5577_xxxx_4706_8787_57d52c03ddf2";
const char* kPodRegex =
    "pod([0-9a-f]{8})[-_]([0-9a-f]{4})[-_]([0-9a-f]{4})[-_]([0-9a-f]{4})[-_]([0-9a-f]{12})";

// NOLINTNEXTLINE : runtime/references.
static void BM_Inline(benchmark::State& state) {
  std::cmatch matches;
  for (auto _ : state) {
    std::regex rgx(kPodRegex);
    std::regex_search(kPodNameMatch, matches, rgx);
    std::regex_search(kPodNameFail, matches, rgx);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_StaticInline(benchmark::State& state) {
  std::cmatch matches;
  for (auto _ : state) {
    static std::regex rgx(kPodRegex);
    std::regex_search(kPodNameMatch, matches, rgx);
    std::regex_search(kPodNameFail, matches, rgx);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_ConstInline(benchmark::State& state) {
  std::cmatch matches;
  for (auto _ : state) {
    const std::regex rgx(kPodRegex);
    std::regex_search(kPodNameMatch, matches, rgx);
    std::regex_search(kPodNameFail, matches, rgx);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_ConstStaticInline(benchmark::State& state) {
  std::cmatch matches;
  for (auto _ : state) {
    static std::regex rgx(kPodRegex);
    std::regex_search(kPodNameMatch, matches, rgx);
    std::regex_search(kPodNameFail, matches, rgx);
  }
}

static std::regex kGlobalRegex(kPodRegex);
// NOLINTNEXTLINE : runtime/references.
static void BM_Global(benchmark::State& state) {
  std::cmatch matches;
  for (auto _ : state) {
    std::regex_search(kPodNameMatch, matches, kGlobalRegex);
    std::regex_search(kPodNameFail, matches, kGlobalRegex);
  }
}

BENCHMARK(BM_Inline);
BENCHMARK(BM_StaticInline);
BENCHMARK(BM_ConstInline);
BENCHMARK(BM_ConstStaticInline);
BENCHMARK(BM_Global);

}  // namespace px
