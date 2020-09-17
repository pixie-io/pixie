#include "benchmark/benchmark.h"
#include "src/common/base/base.h"

int main(int argc, char** argv) {
  // Initialize must come before env_guard otherwise the arguments are not declared and env_guard
  // throws an error if you pass in benchmark args. Note the arguments that benchmark uses are also
  // consumed but doesn't affect other arguments.
  benchmark::Initialize(&argc, argv);
  pl::EnvironmentGuard env_guard(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
