#include "benchmark/benchmark.h"
#include "src/common/base/base.h"

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  pl::ShutdownEnvironmentOrDie();
  return 0;
}
