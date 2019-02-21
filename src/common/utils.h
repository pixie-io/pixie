#pragma once

#include <unistd.h>

namespace pl {

inline bool IsRoot() { return (geteuid() == 0); }

}  // namespace pl
