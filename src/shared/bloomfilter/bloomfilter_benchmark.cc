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

#include <absl/container/flat_hash_map.h>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

#include "src/datagen/datagen.h"
#include "src/shared/bloomfilter/bloomfilter.h"

namespace px {
namespace bloomfilter {

class BloomFilterBenchmark : public benchmark::Fixture {
  void SetUp(const ::benchmark::State& state) {
    auto num_items = state.range(0);
    auto error_rate = 1.0 / state.range(1);
    auto strlen = state.range(2);
    insert_bf_ = XXHash64BloomFilter::Create(num_items * 2, error_rate).ConsumeValueOrDie();
    lookup_bf_ = XXHash64BloomFilter::Create(num_items * 2, error_rate).ConsumeValueOrDie();
    random_strs_.reserve(num_items);
    for (auto i = 0; i < num_items; ++i) {
      random_strs_.push_back(datagen::RandomString(strlen));
      lookup_bf_->Insert(random_strs_[i]);
    }
  }

 protected:
  std::vector<std::string> random_strs_;
  std::unique_ptr<XXHash64BloomFilter> insert_bf_;
  std::unique_ptr<XXHash64BloomFilter> lookup_bf_;
};

// NOLINTNEXTLINE : runtime/references.
BENCHMARK_DEFINE_F(BloomFilterBenchmark, InsertTest)(benchmark::State& state) {
  for (auto _ : state) {
    for (const auto& random_str : random_strs_) {
      insert_bf_->Insert(random_str);
    }
  }
  state.SetBytesProcessed(state.iterations() * random_strs_.size() * random_strs_[0].size());
  state.SetItemsProcessed(state.iterations() * random_strs_.size());
}

// NOLINTNEXTLINE : runtime/references.
BENCHMARK_DEFINE_F(BloomFilterBenchmark, LookupTest)(benchmark::State& state) {
  bool result = false;
  for (auto _ : state) {
    for (const auto& random_str : random_strs_) {
      result = lookup_bf_->Contains(random_str);
    }
  }
  PX_UNUSED(result);
  state.SetBytesProcessed(state.iterations() * random_strs_.size() * random_strs_[0].size());
  state.SetItemsProcessed(state.iterations() * random_strs_.size());
}

BENCHMARK_REGISTER_F(BloomFilterBenchmark, InsertTest)
    ->Ranges({{1 << 10, 1 << 20}, {10, 100000}, {8, 256}});
BENCHMARK_REGISTER_F(BloomFilterBenchmark, LookupTest)
    ->Ranges({{1 << 10, 1 << 20}, {10, 100000}, {8, 256}});

}  // namespace bloomfilter
}  // namespace px
