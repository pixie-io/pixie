#include "benchmark/benchmark.h"
#include "src/common/base/base.h"

int main(int argc, char** argv) {
  pl::EnvironmentGuard env_guard(&argc, argv);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
