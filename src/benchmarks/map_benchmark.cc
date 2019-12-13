#include <benchmark/benchmark.h>

#include <absl/container/flat_hash_map.h>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

template <typename T>
std::vector<T> GenerateRandomVector(uint64_t count) {
  std::vector<T> data;
  data.reserve(count);

  std::seed_seq seed = {123};
  std::mt19937 gen(seed);
  std::uniform_int_distribution<> dis(0, 10000);

  for (uint64_t i = 0; i < count; ++i) {
    data.emplace_back(dis(gen));
  }
  return data;
}

template <typename TMap>
// NOLINTNEXTLINE : runtime/references.
static void BM_InsertRandomNumericKeys(benchmark::State& state) {
  int64_t count = state.range(0);
  auto data = GenerateRandomVector<typename TMap::key_type>(count);

  TMap myMap;
  for (auto _ : state) {
    for (auto d : data) {
      myMap[d] = d;
    }
    benchmark::DoNotOptimize(myMap);
    myMap.clear();
  }
  state.SetItemsProcessed(state.iterations() * count);
}

template <typename TMap>
// NOLINTNEXTLINE : runtime/references.
static void BM_ReadRandomNumericKeys(benchmark::State& state) {
  int64_t count = state.range(0);
  auto data = GenerateRandomVector<typename TMap::key_type>(count);

  TMap myMap;
  for (auto d : data) {
    myMap[d] = d;
  }

  for (auto _ : state) {
    for (auto d : data) {
      typename TMap::mapped_type val = myMap[d];
      benchmark::DoNotOptimize(val);
    }
  }
  state.SetItemsProcessed(state.iterations() * count);
}

template <typename TMap>
// NOLINTNEXTLINE : runtime/references.
static void BM_CountKeys(benchmark::State& state) {
  int64_t count = state.range(0);
  auto data = GenerateRandomVector<typename TMap::key_type>(count);

  TMap myMap;
  for (auto _ : state) {
    for (auto d : data) {
      myMap[d]++;
    }
    benchmark::DoNotOptimize(myMap);
    myMap.clear();
  }
  state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK_TEMPLATE(BM_InsertRandomNumericKeys, std::unordered_map<int64_t, int64_t>)
    ->Range(1 << 14, 1 << 24);
BENCHMARK_TEMPLATE(BM_InsertRandomNumericKeys, std::map<int64_t, int64_t>)->Range(1 << 14, 1 << 24);
BENCHMARK_TEMPLATE(BM_InsertRandomNumericKeys, absl::flat_hash_map<int64_t, int64_t>)
    ->Range(1 << 14, 1 << 24);

BENCHMARK_TEMPLATE(BM_ReadRandomNumericKeys, std::unordered_map<int64_t, int64_t>)
    ->Range(1 << 14, 1 << 24);
BENCHMARK_TEMPLATE(BM_ReadRandomNumericKeys, std::map<int64_t, int64_t>)->Range(1 << 14, 1 << 24);
BENCHMARK_TEMPLATE(BM_ReadRandomNumericKeys, absl::flat_hash_map<int64_t, int64_t>)
    ->Range(1 << 14, 1 << 24);

BENCHMARK_TEMPLATE(BM_CountKeys, std::unordered_map<int64_t, int64_t>)->Range(1 << 14, 1 << 24);
BENCHMARK_TEMPLATE(BM_CountKeys, std::map<int64_t, int64_t>)->Range(1 << 14, 1 << 24);
BENCHMARK_TEMPLATE(BM_CountKeys, absl::flat_hash_map<int64_t, int64_t>)->Range(1 << 14, 1 << 24);
