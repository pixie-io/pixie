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

#include <random>
#include <vector>
#include "src/common/benchmark/benchmark.h"
#include "src/datagen/datagen.h"
#include "src/shared/types/types.h"

using px::types::Int64Value;

// This is just a dummy function that does some work so we can use it in the benchmark.
template <typename T>
std::vector<T> Compute(const std::vector<T>& vec1, const std::vector<T>& vec2) {
  size_t size = std::min(vec1.size(), vec2.size());
  std::vector<T> res(size);

  for (size_t i = 0; i < size; ++i) {
    res[i] = (vec1[i] * vec2[i] + vec1[i] + vec2[i]);
  }
  return res;
}

// Specialization of the above function for Int64Value (since it needs accessors).
template <>
std::vector<Int64Value> Compute(const std::vector<Int64Value>& vec1,
                                const std::vector<Int64Value>& vec2) {
  size_t size = std::min(vec1.size(), vec2.size());
  std::vector<Int64Value> res(size);

  for (size_t i = 0; i < size; ++i) {
    res[i] = vec1[i].val * vec2[i].val + vec1[i].val + vec2[i].val;
  }
  return res;
}

template <typename T>
static void BM_Int64Vector(benchmark::State& state) {  // NOLINT
  auto vec1 = px::datagen::CreateLargeData<T>(state.range(0), 1, 52);
  auto vec2 = px::datagen::CreateLargeData<T>(state.range(0), 1, 52);

  for (auto _ : state) {
    auto res = Compute(vec1, vec2);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * 2 * vec1.size());
}

BENCHMARK_TEMPLATE(BM_Int64Vector, int64_t)->Arg(10000);
BENCHMARK_TEMPLATE(BM_Int64Vector, Int64Value)->Arg(10000);
