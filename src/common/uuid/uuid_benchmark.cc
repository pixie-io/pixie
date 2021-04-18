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

#include <sole.hpp>

namespace px {

// NOLINTNEXTLINE : runtime/references.
static void BM_UUIDFromString(benchmark::State& state) {
  std::string uustr = "ea8aa095-697f-49f1-b127-d50e5b6e2645";
  for (auto _ : state) {
    benchmark::DoNotOptimize(sole::rebuild(uustr));
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_UUIDFromBytes(benchmark::State& state) {
  uint64_t high_bits = 0xea8aa095697f49f1;
  uint64_t low_bits = 0xb127d50e5b6e2645;
  for (auto _ : state) {
    benchmark::DoNotOptimize(sole::rebuild(high_bits, low_bits));
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_UUIDToString(benchmark::State& state) {
  auto uuid = sole::uuid4();
  for (auto _ : state) {
    benchmark::DoNotOptimize(uuid.str());
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_UUIDToBytes(benchmark::State& state) {
  auto uuid = sole::uuid4();
  for (auto _ : state) {
    benchmark::DoNotOptimize(uuid.ab);
    benchmark::DoNotOptimize(uuid.cd);
  }
}

BENCHMARK(BM_UUIDFromString);
BENCHMARK(BM_UUIDFromBytes);
BENCHMARK(BM_UUIDToString);
BENCHMARK(BM_UUIDToBytes);
}  // namespace px
