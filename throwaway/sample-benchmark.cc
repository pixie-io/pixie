#include <benchmark/benchmark.h>
#include <string>

// NOLINTNEXTLINE : runtime/references.
static void BM_StringCreation(benchmark::State& state) {
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) std::string empty_string;
}
// Register the function as a benchmark
BENCHMARK(BM_StringCreation);

// NOLINTNEXTLINE : runtime/references.
static void BM_StringCopy(benchmark::State& state) {
  std::string x = "hello";
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) std::string copy(x);
}
BENCHMARK(BM_StringCopy);

BENCHMARK_MAIN();
