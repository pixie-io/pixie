// Defines basic execution environment related stuff.
#pragma once

#include "src/common/logging.h"

namespace pl {

// Does basic environment init (such as flags and logging).
// Can only be called once. Multiple invocations are ignored.
void InitEnvironmentOrDie(int *argc, char **argv);

// Cleans up the environment.
// Can only be called once. Multiple invocations are ignored.
void ShutdownEnvironmentOrDie();

}  // namespace pl
