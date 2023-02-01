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

#include <absl/container/flat_hash_map.h>
#include <benchmark/benchmark.h>

#include <map>

#include "src/common/base/base.h"

#define COMPUTE_VALUE(x) uint64_t(x) ^ 0xDEADBEEFDEADBEEFULL

template <typename T>
void GenerateMapData(T* _m, const uint32_t num_elements) {
  // Lint requires that non-const be passed as a pointer.
  // Just below, pointer syntax gets awkward. Here, we just convert it back to a ref.
  T& m = *_m;

  for (uint32_t i = 0; i < num_elements; ++i) {
    m[i] = COMPUTE_VALUE(i);
  }
}

template <typename T>
void GenerateVectorData(T* v, const uint32_t num_elements) {
  for (uint32_t i = 0; i < num_elements; ++i) {
    v->push_back(COMPUTE_VALUE(i));
  }
}

template <typename T>
void map_iterate(benchmark::State* state) {
  const uint64_t num_maps = state->range(1);
  std::vector<T> ms(num_maps);

  for (uint64_t i = 0; i < num_maps; ++i) {
    GenerateMapData(&ms[i], state->range(0));
  }

  for (auto _ : *state) {
    for (uint64_t i = 0; i < num_maps; ++i) {
      T& m = ms[i];
      for (const auto& [key, val] : m) {
        const uint64_t expected_value = COMPUTE_VALUE(key);
        ECHECK_EQ(val, expected_value);
      }
    }
  }
}

template <typename T>
void vector_iterate(benchmark::State* state) {
  const uint64_t num_vecs = state->range(1);
  std::vector<T> vs(num_vecs);

  for (uint64_t i = 0; i < num_vecs; ++i) {
    GenerateVectorData(&vs[i], state->range(0));
  }

  for (const auto _ : *state) {
    for (uint64_t i = 0; i < num_vecs; ++i) {
      uint32_t key = 0;
      T& v = vs[i];
      for (const auto& val : v) {
        const uint64_t expected_value = COMPUTE_VALUE(key);
        ECHECK_EQ(val, expected_value);
        ++key;
      }
    }
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_std_map(benchmark::State& state) {
  map_iterate<std::map<uint32_t, uint64_t> >(&state);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_std_unordered_map(benchmark::State& state) {
  map_iterate<std::unordered_map<uint32_t, uint64_t> >(&state);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_absl_flat_hash_map(benchmark::State& state) {
  map_iterate<absl::flat_hash_map<uint32_t, uint64_t> >(&state);
}

// NOLINTNEXTLINE : runtime/references.
static void BM_std_vector(benchmark::State& state) {
  vector_iterate<std::vector<uint64_t> >(&state);
}

constexpr uint64_t total_data_set_size = 1ULL << 20;
constexpr uint64_t num_elems_per_container = 16;
constexpr uint64_t num_containers = total_data_set_size / num_elems_per_container;

// Setup two experiments (both involve a small number of elements in the individual container):
// 1. ooc "out-of-cache": We have a total data set that is too big to fit in the CPU cache.
// 2. cached: We have just one container with a small number of elements.
const std::vector<int64_t> ooc_args = {num_elems_per_container, num_containers};
const std::vector<int64_t> cached_args = {num_elems_per_container, 1};

// To show out-of-cache behavior, we create many containers each with "num_elems" elements.
BENCHMARK(BM_std_vector)->Name("std::vector/out-of-cache")->Args(ooc_args);
BENCHMARK(BM_absl_flat_hash_map)->Name("absl::flat_hash_map/out-of-cache")->Args(ooc_args);
BENCHMARK(BM_std_map)->Name("std::map/out-of-cache")->Args(ooc_args);
BENCHMARK(BM_std_unordered_map)->Name("std::unordered_map/out-of-cache")->Args(ooc_args);

// To show cached behavior (this is not the expected case that we should consider),
// we create one container of size "num_elems".
BENCHMARK(BM_std_vector)->Name("std::vector/cached")->Args(cached_args);
BENCHMARK(BM_absl_flat_hash_map)->Name("absl::flat_hash_map/cached")->Args(cached_args);
BENCHMARK(BM_std_map)->Name("std::map/cached")->Args(cached_args);
BENCHMARK(BM_std_unordered_map)->Name("std::unordered_map/cached")->Args(cached_args);
