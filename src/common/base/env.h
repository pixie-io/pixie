// Defines basic execution environment related stuff.
#pragma once

#include <sys/resource.h>
#include <sys/time.h>

#include <chrono>
#include <string>

#include "src/common/base/logging.h"

namespace px {

// A RAII-style wrapper of initializing and shutting down the process execution environment
// (such as flags and logging).
class EnvironmentGuard {
 public:
  EnvironmentGuard(int* argc, char** argv);
  ~EnvironmentGuard();
};

// A class to monitor CPU usage of the process.
// Instantiate this at the beginning of your program,
// and it will print out stats when it falls out of scope, at exit.
class ProcessStatsMonitor {
 public:
  ProcessStatsMonitor() { Reset(); }

  ~ProcessStatsMonitor() { PrintCPUTime(); }

  void Reset();

  void PrintCPUTime();

 private:
  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  struct rusage start_usage_;
};

// Returns the value of the env var. Or nullopt if it's not set.
std::optional<std::string> GetEnv(const std::string& env_var);

void ChDirPixieRoot();

}  // namespace px
