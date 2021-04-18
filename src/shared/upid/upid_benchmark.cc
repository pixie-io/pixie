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

#include <absl/container/flat_hash_set.h>
#include <benchmark/benchmark.h>
#include <random>

#include "src/common/benchmark/benchmark.h"
#include "src/shared/upid/upid.h"

using ::px::md::UPID;

template <typename UpidSetType>
static void BM_set_insertion(benchmark::State& state) {  // NOLINT
  UpidSetType upid_set;
  for (auto _ : state) {
    for (int64_t i = 0; i < state.range(0); ++i) {
      upid_set.insert(UPID{static_cast<uint32_t>(i), static_cast<uint32_t>(i), i});
    }
  }
}

BENCHMARK_TEMPLATE(BM_set_insertion, std::set<UPID>)->DenseRange(100, 1000, 100);
BENCHMARK_TEMPLATE(BM_set_insertion, absl::flat_hash_set<UPID>)->DenseRange(100, 1000, 100);
