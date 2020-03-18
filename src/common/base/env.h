// Defines basic execution environment related stuff.
#pragma once

#include <string>

#include "src/common/base/logging.h"

namespace pl {

// A RAII-style wrapper of initializing and shutting down the process execution environment
// (such as flags and logging).
class EnvironmentGuard {
 public:
  EnvironmentGuard(int* argc, char** argv);
  ~EnvironmentGuard();
};

// Returns the value of the env var. Or nullopt if it's not set.
std::optional<std::string> GetEnv(const std::string& env_var);

void ChDirPixieRoot();

}  // namespace pl
