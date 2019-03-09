#include <benchmark/benchmark.h>

#include <random>
#include <vector>
#include "src/shared/types/types.h"

using pl::types::Int64Value;

template <typename T>
std::vector<T> CreateLargeData(int size) {
  std::vector<T> data(size);

  std::random_device rnd_device;
  std::mt19937 mersenne_engine{rnd_device()};  // Generates random integers
  std::uniform_int_distribution<int64_t> dist{1, 52};

  auto gen = [&dist, &mersenne_engine]() { return dist(mersenne_engine); };

  std::generate(begin(data), end(data), gen);
  return data;
}

// This is just a dummy function that does some work so we can use it in the benchmark.
template <typename T>
std::vector<T> Compute(const std::vector<T> &vec1, const std::vector<T> &vec2) {
  size_t size = std::min(vec1.size(), vec2.size());
  std::vector<T> res(size);

  for (size_t i = 0; i < size; ++i) {
    res[i] = (vec1[i] * vec2[i] + vec1[i] + vec2[i]);
  }
  return res;
}

// Specialization of the above function for Int64Value (since it needs accessors).
template <>
std::vector<Int64Value> Compute(const std::vector<Int64Value> &vec1,
                                const std::vector<Int64Value> &vec2) {
  size_t size = std::min(vec1.size(), vec2.size());
  std::vector<Int64Value> res(size);

  for (size_t i = 0; i < size; ++i) {
    res[i] = vec1[i].val * vec2[i].val + vec1[i].val + vec2[i].val;
  }
  return res;
}

template <typename T>
static void BM_Int64Vector(benchmark::State &state) {  // NOLINT
  auto vec1 = CreateLargeData<T>(state.range(0));
  auto vec2 = CreateLargeData<T>(state.range(0));

  for (auto _ : state) {
    auto res = Compute(vec1, vec2);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * 2 * vec1.size());
}

BENCHMARK_TEMPLATE(BM_Int64Vector, int64_t)->Arg(10000);
BENCHMARK_TEMPLATE(BM_Int64Vector, Int64Value)->Arg(10000);
