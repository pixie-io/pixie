#pragma once
#include <string>
// This header file should be used for all logging.

// Include gflags before glog so flags get set correctly.
#include <gflags/gflags.h>
#include <glog/logging.h>

namespace pl {

// Indent provides a consitent indent base on level.
inline std::string Indent(int level) { return std::string(level, '\t'); }

}  // namespace pl
