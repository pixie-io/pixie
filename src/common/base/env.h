// Defines basic execution environment related stuff.
#pragma once

#include <string>

#include "src/common/base/logging.h"

namespace pl {

// Does basic environment init (such as flags and logging).
// Can only be called once. Multiple invocations are ignored.
void InitEnvironmentOrDie(int* argc, char** argv);

// Cleans up the environment.
// Can only be called once. Multiple invocations are ignored.
void ShutdownEnvironmentOrDie();

// Returns the value of the env var. Or nullopt if it's not set.
std::optional<std::string> GetEnv(const std::string& env_var);

}  // namespace pl
