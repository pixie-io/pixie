#pragma once
// This header file should be used for all logging.

// Include gflags before glog so flags get set correctly.
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <string>

// A new form of CHECK is defined below, to extend those in the glog implementation.
// The following table shows the behavioral differences.
//
// MODE               DEBUG           PRODUCTION
// -------------------------------------------------------
// DCHECK             FATAL           nothing
// CHECK              FATAL           FATAL
// ECHECK             FATAL           ERROR

#define ECHECK(condition) \
  LOG_IF(DFATAL, GOOGLE_PREDICT_BRANCH_NOT_TAKEN(!(condition))) << "Check failed: " #condition " "

namespace pl {

// Indent provides a consitent indent base on level.
inline std::string Indent(int level) { return std::string(level, '\t'); }

}  // namespace pl
