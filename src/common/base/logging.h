/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "src/common/base/macros.h"  // NOLINT(build/include_order).
// This header file should be used for all logging.

// Include gflags before glog so flags get set correctly.
PX_SUPPRESS_WARNINGS_START()
#include <gflags/gflags.h>     // NOLINT(build/include_order).
#include <glog/logging.h>      // NOLINT(build/include_order).
#include <glog/stl_logging.h>  // NOLINT(build/include_order).
PX_SUPPRESS_WARNINGS_END()

#include <string>

#ifdef THREAD_SANITIZER
// Disable VLOGs in TSAN mode. Glog has a benign race on VLOG initialization which might be fixed in
// a newer version but we can't upgrade because the newer version conflicts with abseil.
#undef VLOG
#define VLOG(n) LOG_IF(INFO, false)
#endif

// A new form of CHECK is defined below, to extend those in the glog implementation.
// The following table shows the behavioral differences.
//
// MODE               DEBUG           PRODUCTION
// -------------------------------------------------------
// DCHECK             FATAL           nothing
// CHECK              FATAL           FATAL
// ECHECK             FATAL           ERROR
//
// It is essentially a short-hand for LOG(DFATAL).

// NOTE: Macros copied from glog/logging.h, and simplified.
//       They may not be as optimal as those in glog.
// TODO(oazizi): Consider optimizations (compare to glog).

#define ECHECK(condition) \
  LOG_IF(DFATAL, GOOGLE_PREDICT_BRANCH_NOT_TAKEN(!(condition))) << "Check failed: " #condition " "

#define ECHECK_OK(status) \
  LOG_IF(DFATAL, GOOGLE_PREDICT_BRANCH_NOT_TAKEN(!(status.ok()))) << "Check failed: " #status " "

// In optimized mode, CheckOpString provides to hint to compiler that
// the if statement is unlikely.
#define ECHECK_OP_LOG(name, op, val1, val2)                                                        \
  if (google::CheckOpString _result =                                                              \
          google::Check##name##Impl(google::GetReferenceableValue(val1),                           \
                                    google::GetReferenceableValue(val2), #val1 " " #op " " #val2)) \
  LOG(DFATAL) << "Check failed: " << (*_result.str_) << " "

#define ECHECK_OP(name, op, val1, val2) ECHECK_OP_LOG(name, op, val1, val2)

#define ECHECK_EQ(val1, val2) ECHECK_OP(_EQ, ==, val1, val2)
#define ECHECK_NE(val1, val2) ECHECK_OP(_NE, !=, val1, val2)
#define ECHECK_LE(val1, val2) ECHECK_OP(_LE, <=, val1, val2)
#define ECHECK_LT(val1, val2) ECHECK_OP(_LT, <, val1, val2)
#define ECHECK_GE(val1, val2) ECHECK_OP(_GE, >=, val1, val2)
#define ECHECK_GT(val1, val2) ECHECK_OP(_GT, >, val1, val2)

namespace px {

// Indent provides a consitent indent base on level.
inline std::string Indent(int level) { return std::string(level, '\t'); }

}  // namespace px
