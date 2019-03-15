#include "benchmark/benchmark.h"
#include "src/common/common.h"

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  pl::ShutdownEnvironmentOrDie();
  return 0;
}
