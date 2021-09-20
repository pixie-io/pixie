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

#include "src/common/clock/clock_conversion.h"

namespace {
template <size_t TCapacity>
static inline std::unique_ptr<px::clock::InterpolatingLookupTable<TCapacity>> InitTable(
    uint64_t base_val) {
  auto table = std::make_unique<px::clock::InterpolatingLookupTable<TCapacity>>();
  for (size_t i = 0; i < TCapacity; ++i) {
    table->Emplace(base_val + 100 * i, base_val + 100 * i + 100);
  }
  return table;
}
}  // namespace

namespace px {
namespace clock {

// NOLINTNEXTLINE : runtime/references.
void BM_InterpolatingLookupTableGet(benchmark::State& state) {
  constexpr size_t capacity =
      ClockConverter::BufferCapacity(DefaultMonoToRealtimeConverter::kUpdatePeriod);
  uint64_t base_val = 1640000000271885073;
  auto table = InitTable<capacity>(base_val);

  for (auto _ : state) {
    benchmark::DoNotOptimize(table->Get(base_val + 100 * capacity / 2));
  }
}
// NOLINTNEXTLINE : runtime/references.
void BM_InterpolatingLookupTableEmplace(benchmark::State& state) {
  constexpr size_t capacity =
      ClockConverter::BufferCapacity(DefaultMonoToRealtimeConverter::kUpdatePeriod);
  uint64_t base_val = 1640000000271885073;
  auto table = InitTable<capacity>(base_val);

  for (auto _ : state) {
    table->Emplace(base_val, base_val + 1);
  }
}

BENCHMARK(BM_InterpolatingLookupTableGet);
BENCHMARK(BM_InterpolatingLookupTableEmplace);

}  // namespace clock
}  // namespace px
