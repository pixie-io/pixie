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
